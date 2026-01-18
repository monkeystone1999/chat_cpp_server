#include "crypt.hpp"
#include <cstring>
#include <iostream>
#include <openssl/rand.h>

namespace crypt_server {

// ============================================================================
// Static 멤버 초기화
// ============================================================================
bool CryptTLS::ssl_initialized_ = false;
bool CryptDTLS::ssl_initialized_ = false;
unsigned char CryptDTLS::cookie_secret_[16] = {0};
bool CryptDTLS::cookie_initialized_ = false;

// OpenSSL 라이브러리 초기화
void CryptTLS::initOpenSSL() {
  if (!ssl_initialized_) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    ssl_initialized_ = true;
  }
}

void CryptDTLS::initOpenSSL() {
  if (!ssl_initialized_) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    ssl_initialized_ = true;
  }
}

// ============================================================================
// CryptTLS 구현 (TCP용 TLS)
// ============================================================================

CryptTLS::CryptTLS(const std::string& certFile, const std::string& keyFile,
                   const std::string& caFile, bool isServer)
    : ctx_(nullptr), ssl_(nullptr), rbio_(nullptr), wbio_(nullptr),
      connected_(false), isServer_(isServer), fd_(-1) {
  initOpenSSL();
  
  // TLS 컨텍스트 생성
  if (isServer_) {
    ctx_ = SSL_CTX_new(TLS_server_method());
  } else {
    ctx_ = SSL_CTX_new(TLS_client_method());
  }
  
  if (!ctx_) {
    throw std::runtime_error("SSL_CTX_new 실패: " + getSSLError());
  }
  
  // TLS 1.2 이상만 허용
  SSL_CTX_set_min_proto_version(ctx_, TLS1_2_VERSION);
  
  // 인증서 로드
  if (loadCertificates(certFile, keyFile, caFile) < 0) {
    SSL_CTX_free(ctx_);
    throw std::runtime_error("인증서 로드 실패");
  }
}

CryptTLS::~CryptTLS() {
  sslDisconnect();
  if (ctx_) {
    SSL_CTX_free(ctx_);
    ctx_ = nullptr;
  }
}

int CryptTLS::loadCertificates(const std::string& certFile, const std::string& keyFile,
                                const std::string& caFile) {
  // 인증서 로드
  if (SSL_CTX_use_certificate_file(ctx_, certFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
    std::cerr << "인증서 파일 로드 실패: " << certFile << " - " << getSSLError() << std::endl;
    return -1;
  }
  
  // 개인키 로드
  if (SSL_CTX_use_PrivateKey_file(ctx_, keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
    std::cerr << "개인키 파일 로드 실패: " << keyFile << " - " << getSSLError() << std::endl;
    return -1;
  }
  
  // 개인키-인증서 매칭 확인
  if (!SSL_CTX_check_private_key(ctx_)) {
    std::cerr << "인증서와 개인키가 일치하지 않음" << std::endl;
    return -1;
  }
  
  // CA 인증서 로드 (상호 인증용)
  if (!caFile.empty()) {
    if (SSL_CTX_load_verify_locations(ctx_, caFile.c_str(), nullptr) <= 0) {
      std::cerr << "CA 인증서 로드 실패: " << caFile << std::endl;
      return -1;
    }
    // 클라이언트 인증서 검증 요구 (서버 모드)
    if (isServer_) {
      SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    } else {
      SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
    }
  }
  
  return 0;
}

int CryptTLS::sslConnect(int fd) {
  if (connected_) return -1;
  
  ssl_ = SSL_new(ctx_);
  if (!ssl_) return -1;
  
  fd_ = fd;
  SSL_set_fd(ssl_, fd);
  
  int ret = SSL_connect(ssl_);
  if (ret != 1) {
    int err = SSL_get_error(ssl_, ret);
    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
      std::cerr << "SSL_connect 실패: " << getSSLError() << std::endl;
      SSL_free(ssl_);
      ssl_ = nullptr;
      return -1;
    }
  }
  
  connected_ = true;
  return 0;
}

int CryptTLS::sslAccept(int fd) {
  if (connected_) return -1;
  
  ssl_ = SSL_new(ctx_);
  if (!ssl_) return -1;
  
  fd_ = fd;
  SSL_set_fd(ssl_, fd);
  
  int ret = SSL_accept(ssl_);
  if (ret != 1) {
    int err = SSL_get_error(ssl_, ret);
    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
      std::cerr << "SSL_accept 실패: " << getSSLError() << std::endl;
      SSL_free(ssl_);
      ssl_ = nullptr;
      return -1;
    }
  }
  
  connected_ = true;
  return 0;
}

int CryptTLS::sslDisconnect() {
  if (!connected_ || !ssl_) return 0;
  
  SSL_shutdown(ssl_);
  SSL_free(ssl_);
  ssl_ = nullptr;
  rbio_ = nullptr;
  wbio_ = nullptr;
  connected_ = false;
  fd_ = -1;
  
  return 0;
}

int CryptTLS::sslRead(std::vector<uint8_t>& buffer, size_t maxLen) {
  if (!connected_ || !ssl_) return -1;
  
  buffer.resize(maxLen);
  int read = SSL_read(ssl_, buffer.data(), static_cast<int>(maxLen));
  
  if (read <= 0) {
    int err = SSL_get_error(ssl_, read);
    buffer.clear();
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;
    if (err == SSL_ERROR_ZERO_RETURN) return 0;
    return -1;
  }
  
  buffer.resize(read);
  return read;
}

int CryptTLS::sslRead(std::string& buffer, size_t maxLen) {
  std::vector<uint8_t> temp;
  int ret = sslRead(temp, maxLen);
  if (ret > 0) buffer.assign(temp.begin(), temp.end());
  else buffer.clear();
  return ret;
}

int CryptTLS::sslWrite(const std::vector<uint8_t>& data) {
  if (!connected_ || !ssl_) return -1;
  
  int written = SSL_write(ssl_, data.data(), static_cast<int>(data.size()));
  if (written <= 0) {
    int err = SSL_get_error(ssl_, written);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;
    return -1;
  }
  return written;
}

int CryptTLS::sslWrite(const std::string& data) {
  std::vector<uint8_t> temp(data.begin(), data.end());
  return sslWrite(temp);
}

int CryptTLS::bioRead(std::vector<uint8_t>& buffer, size_t maxLen) {
  if (!wbio_) return -1;
  buffer.resize(maxLen);
  int read = BIO_read(wbio_, buffer.data(), static_cast<int>(maxLen));
  if (read <= 0) { buffer.clear(); return 0; }
  buffer.resize(read);
  return read;
}

int CryptTLS::bioWrite(const std::vector<uint8_t>& data) {
  if (!rbio_) return -1;
  return BIO_write(rbio_, data.data(), static_cast<int>(data.size()));
}

std::string CryptTLS::getSSLError() const {
  unsigned long err = ERR_get_error();
  if (err == 0) return "Unknown error";
  char buf[256];
  ERR_error_string_n(err, buf, sizeof(buf));
  return std::string(buf);
}

// ============================================================================
// CryptDTLS 구현 (UDP용 DTLS)
// ============================================================================

CryptDTLS::CryptDTLS(const std::string& certFile, const std::string& keyFile,
                     const std::string& caFile, bool isServer)
    : ctx_(nullptr), ssl_(nullptr), bio_(nullptr),
      connected_(false), isServer_(isServer), fd_(-1) {
  initOpenSSL();
  
  // DTLS 컨텍스트 생성
  if (isServer_) {
    ctx_ = SSL_CTX_new(DTLS_server_method());
  } else {
    ctx_ = SSL_CTX_new(DTLS_client_method());
  }
  
  if (!ctx_) {
    throw std::runtime_error("DTLS SSL_CTX_new 실패: " + getSSLError());
  }
  
  // DTLS 1.2 이상만 허용
  SSL_CTX_set_min_proto_version(ctx_, DTLS1_2_VERSION);
  
  // 인증서 로드
  if (loadCertificates(certFile, keyFile, caFile) < 0) {
    SSL_CTX_free(ctx_);
    throw std::runtime_error("인증서 로드 실패");
  }
}

CryptDTLS::~CryptDTLS() {
  dtlsDisconnect();
  if (ctx_) {
    SSL_CTX_free(ctx_);
    ctx_ = nullptr;
  }
}

int CryptDTLS::loadCertificates(const std::string& certFile, const std::string& keyFile,
                                 const std::string& caFile) {
  if (SSL_CTX_use_certificate_file(ctx_, certFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
    std::cerr << "DTLS 인증서 파일 로드 실패: " << certFile << std::endl;
    return -1;
  }
  
  if (SSL_CTX_use_PrivateKey_file(ctx_, keyFile.c_str(), SSL_FILETYPE_PEM) <= 0) {
    std::cerr << "DTLS 개인키 파일 로드 실패: " << keyFile << std::endl;
    return -1;
  }
  
  if (!SSL_CTX_check_private_key(ctx_)) {
    std::cerr << "DTLS 인증서와 개인키가 일치하지 않음" << std::endl;
    return -1;
  }
  
  if (!caFile.empty()) {
    if (SSL_CTX_load_verify_locations(ctx_, caFile.c_str(), nullptr) <= 0) {
      std::cerr << "DTLS CA 인증서 로드 실패: " << caFile << std::endl;
      return -1;
    }
    if (isServer_) {
      SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
    } else {
      SSL_CTX_set_verify(ctx_, SSL_VERIFY_PEER, nullptr);
    }
  }
  
  return 0;
}

void CryptDTLS::enableCookieExchange() {
  if (!cookie_initialized_) {
    RAND_bytes(cookie_secret_, sizeof(cookie_secret_));
    cookie_initialized_ = true;
  }
  SSL_CTX_set_cookie_generate_cb(ctx_, generateCookie);
  SSL_CTX_set_cookie_verify_cb(ctx_, verifyCookie);
}

int CryptDTLS::generateCookie(SSL* ssl, unsigned char* cookie, unsigned int* cookieLen) {
  // 간단한 쿠키 생성 (실제로는 HMAC 사용 권장)
  memcpy(cookie, cookie_secret_, sizeof(cookie_secret_));
  *cookieLen = sizeof(cookie_secret_);
  return 1;
}

int CryptDTLS::verifyCookie(SSL* ssl, const unsigned char* cookie, unsigned int cookieLen) {
  if (cookieLen != sizeof(cookie_secret_)) return 0;
  return memcmp(cookie, cookie_secret_, sizeof(cookie_secret_)) == 0 ? 1 : 0;
}

int CryptDTLS::dtlsConnect(int fd) {
  if (connected_) return -1;
  
  ssl_ = SSL_new(ctx_);
  if (!ssl_) return -1;
  
  fd_ = fd;
  
  // UDP 소켓용 BIO 생성
  bio_ = BIO_new_dgram(fd, BIO_NOCLOSE);
  if (!bio_) {
    SSL_free(ssl_);
    ssl_ = nullptr;
    return -1;
  }
  
  SSL_set_bio(ssl_, bio_, bio_);
  
  // DTLS 타임아웃 설정
  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 0;
  BIO_ctrl(bio_, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
  
  int ret = SSL_connect(ssl_);
  if (ret != 1) {
    int err = SSL_get_error(ssl_, ret);
    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
      std::cerr << "DTLS_connect 실패: " << getSSLError() << std::endl;
      SSL_free(ssl_);
      ssl_ = nullptr;
      bio_ = nullptr;
      return -1;
    }
  }
  
  connected_ = true;
  return 0;
}

int CryptDTLS::dtlsAccept(int fd, struct sockaddr* peerAddr, socklen_t addrLen) {
  if (connected_) return -1;
  
  ssl_ = SSL_new(ctx_);
  if (!ssl_) return -1;
  
  fd_ = fd;
  
  // UDP 소켓용 BIO 생성
  bio_ = BIO_new_dgram(fd, BIO_NOCLOSE);
  if (!bio_) {
    SSL_free(ssl_);
    ssl_ = nullptr;
    return -1;
  }
  
  SSL_set_bio(ssl_, bio_, bio_);
  
  // 상대방 주소 설정
  BIO_ctrl(bio_, BIO_CTRL_DGRAM_SET_CONNECTED, 0, peerAddr);
  
  // DTLS 타임아웃 설정
  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 0;
  BIO_ctrl(bio_, BIO_CTRL_DGRAM_SET_RECV_TIMEOUT, 0, &timeout);
  
  // Listen for DTLS ClientHello
  int ret = DTLSv1_listen(ssl_, (BIO_ADDR*)peerAddr);
  if (ret <= 0) {
    std::cerr << "DTLSv1_listen 실패" << std::endl;
    SSL_free(ssl_);
    ssl_ = nullptr;
    bio_ = nullptr;
    return -1;
  }
  
  ret = SSL_accept(ssl_);
  if (ret != 1) {
    int err = SSL_get_error(ssl_, ret);
    if (err != SSL_ERROR_WANT_READ && err != SSL_ERROR_WANT_WRITE) {
      std::cerr << "DTLS_accept 실패: " << getSSLError() << std::endl;
      SSL_free(ssl_);
      ssl_ = nullptr;
      bio_ = nullptr;
      return -1;
    }
  }
  
  connected_ = true;
  return 0;
}

int CryptDTLS::dtlsDisconnect() {
  if (!connected_ || !ssl_) return 0;
  
  SSL_shutdown(ssl_);
  SSL_free(ssl_);  // BIO도 함께 해제
  ssl_ = nullptr;
  bio_ = nullptr;
  connected_ = false;
  fd_ = -1;
  
  return 0;
}

int CryptDTLS::dtlsRead(std::vector<uint8_t>& buffer, size_t maxLen) {
  if (!connected_ || !ssl_) return -1;
  
  buffer.resize(maxLen);
  int read = SSL_read(ssl_, buffer.data(), static_cast<int>(maxLen));
  
  if (read <= 0) {
    int err = SSL_get_error(ssl_, read);
    buffer.clear();
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;
    if (err == SSL_ERROR_ZERO_RETURN) return 0;
    return -1;
  }
  
  buffer.resize(read);
  return read;
}

int CryptDTLS::dtlsRead(std::string& buffer, size_t maxLen) {
  std::vector<uint8_t> temp;
  int ret = dtlsRead(temp, maxLen);
  if (ret > 0) buffer.assign(temp.begin(), temp.end());
  else buffer.clear();
  return ret;
}

int CryptDTLS::dtlsWrite(const std::vector<uint8_t>& data) {
  if (!connected_ || !ssl_) return -1;
  
  int written = SSL_write(ssl_, data.data(), static_cast<int>(data.size()));
  if (written <= 0) {
    int err = SSL_get_error(ssl_, written);
    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;
    return -1;
  }
  return written;
}

int CryptDTLS::dtlsWrite(const std::string& data) {
  std::vector<uint8_t> temp(data.begin(), data.end());
  return dtlsWrite(temp);
}

std::string CryptDTLS::getSSLError() const {
  unsigned long err = ERR_get_error();
  if (err == 0) return "Unknown error";
  char buf[256];
  ERR_error_string_n(err, buf, sizeof(buf));
  return std::string(buf);
}

} // namespace crypt_server
