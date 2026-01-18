#pragma once

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace crypt_server {

// ============================================================================
// TLS Crypt 클래스 (TCP용)
// 인증서 기반 SSL/TLS 연결
// ============================================================================
class CryptTLS {
public:
  // 생성자: 인증서 파일 경로로 초기화
  // @param certFile: 서버/클라이언트 인증서 파일 경로 (.pem)
  // @param keyFile: 개인키 파일 경로 (.pem)
  // @param caFile: CA 인증서 파일 경로 (선택, 상호 인증 시 필요)
  // @param isServer: true=서버모드, false=클라이언트모드
  CryptTLS(const std::string& certFile, const std::string& keyFile,
           const std::string& caFile = "", bool isServer = false);
  
  ~CryptTLS();
  
  // 복사/이동 금지
  CryptTLS(const CryptTLS&) = delete;
  CryptTLS& operator=(const CryptTLS&) = delete;
  CryptTLS(CryptTLS&&) = delete;
  CryptTLS& operator=(CryptTLS&&) = delete;

  // SSL 연결 수립 (클라이언트: connect, 서버: accept)
  int sslConnect(int fd);
  int sslAccept(int fd);
  
  // SSL 연결 종료
  int sslDisconnect();
  
  // SSL 읽기/쓰기
  int sslRead(std::vector<uint8_t>& buffer, size_t maxLen = 4096);
  int sslRead(std::string& buffer, size_t maxLen = 4096);
  int sslWrite(const std::vector<uint8_t>& data);
  int sslWrite(const std::string& data);
  
  // BIO 읽기/쓰기 (비동기 I/O용)
  int bioRead(std::vector<uint8_t>& buffer, size_t maxLen = 4096);
  int bioWrite(const std::vector<uint8_t>& data);
  
  // 연결 상태 확인
  bool isConnected() const { return ssl_ != nullptr && connected_; }
  
  // 소켓 FD 반환
  int getFd() const { return fd_; }

private:
  static void initOpenSSL();
  std::string getSSLError() const;
  int loadCertificates(const std::string& certFile, const std::string& keyFile,
                       const std::string& caFile);

  SSL_CTX* ctx_;
  SSL* ssl_;
  BIO* rbio_;
  BIO* wbio_;
  bool connected_;
  bool isServer_;
  int fd_;
  
  static bool ssl_initialized_;
};

// ============================================================================
// DTLS Crypt 클래스 (UDP용)
// 인증서 기반 DTLS 연결
// ============================================================================
class CryptDTLS {
public:
  // 생성자: 인증서 파일 경로로 초기화
  // @param certFile: 서버/클라이언트 인증서 파일 경로 (.pem)
  // @param keyFile: 개인키 파일 경로 (.pem)
  // @param caFile: CA 인증서 파일 경로 (선택)
  // @param isServer: true=서버모드, false=클라이언트모드
  CryptDTLS(const std::string& certFile, const std::string& keyFile,
            const std::string& caFile = "", bool isServer = false);
  
  ~CryptDTLS();
  
  // 복사/이동 금지
  CryptDTLS(const CryptDTLS&) = delete;
  CryptDTLS& operator=(const CryptDTLS&) = delete;
  CryptDTLS(CryptDTLS&&) = delete;
  CryptDTLS& operator=(CryptDTLS&&) = delete;

  // DTLS 연결 수립
  // @param fd: UDP 소켓 FD
  // @param peerAddr: 상대방 주소 (서버 모드에서 필요)
  // @param addrLen: 주소 구조체 크기
  int dtlsConnect(int fd);
  int dtlsAccept(int fd, struct sockaddr* peerAddr, socklen_t addrLen);
  
  // DTLS 연결 종료
  int dtlsDisconnect();
  
  // DTLS 읽기/쓰기
  int dtlsRead(std::vector<uint8_t>& buffer, size_t maxLen = 4096);
  int dtlsRead(std::string& buffer, size_t maxLen = 4096);
  int dtlsWrite(const std::vector<uint8_t>& data);
  int dtlsWrite(const std::string& data);
  
  // 연결 상태 확인
  bool isConnected() const { return ssl_ != nullptr && connected_; }
  
  // 소켓 FD 반환
  int getFd() const { return fd_; }
  
  // DTLS 쿠키 검증 콜백 설정 (서버용, DoS 방지)
  void enableCookieExchange();

private:
  static void initOpenSSL();
  std::string getSSLError() const;
  int loadCertificates(const std::string& certFile, const std::string& keyFile,
                       const std::string& caFile);
  
  // DTLS 쿠키 콜백 함수
  static int generateCookie(SSL* ssl, unsigned char* cookie, unsigned int* cookieLen);
  static int verifyCookie(SSL* ssl, const unsigned char* cookie, unsigned int cookieLen);

  SSL_CTX* ctx_;
  SSL* ssl_;
  BIO* bio_;
  bool connected_;
  bool isServer_;
  int fd_;
  
  static bool ssl_initialized_;
  static unsigned char cookie_secret_[16];
  static bool cookie_initialized_;
};

} // namespace crypt_server
