#include "db.hpp"
#include <iostream>

namespace db_server {

// ============================================================================
// Database 구현
// ============================================================================

Database::Database(const std::string& host, int port, const std::string& dbname,
                   const std::string& user, const std::string& password,
                   const std::string& sslCert, const std::string& sslKey,
                   const std::string& sslRootCert)
    : conn_(nullptr) {
  
  // 연결 문자열 구성
  std::ostringstream connStr;
  connStr << "host=" << host
          << " port=" << port
          << " dbname=" << dbname
          << " user=" << user
          << " password=" << password;
  
  // SSL 설정 추가
  if (!sslCert.empty() && !sslKey.empty()) {
    connStr << " sslmode=verify-full"
            << " sslcert=" << sslCert
            << " sslkey=" << sslKey;
    
    if (!sslRootCert.empty()) {
      connStr << " sslrootcert=" << sslRootCert;
    }
  } else {
    // SSL 인증서 없으면 암호화만 사용
    connStr << " sslmode=require";
  }
  
  // PostgreSQL 연결
  conn_ = PQconnectdb(connStr.str().c_str());
  
  if (PQstatus(conn_) != CONNECTION_OK) {
    setError(PQerrorMessage(conn_));
    PQfinish(conn_);
    conn_ = nullptr;
    throw std::runtime_error("PostgreSQL 연결 실패: " + lastError_);
  }
  
  std::cout << "PostgreSQL 연결 성공 (SSL: " 
            << (sslCert.empty() ? "암호화만" : "인증서 기반") << ")" << std::endl;
}

Database::~Database() {
  disconnect();
}

bool Database::isConnected() const {
  return conn_ != nullptr && PQstatus(conn_) == CONNECTION_OK;
}

void Database::disconnect() {
  if (conn_) {
    PQfinish(conn_);
    conn_ = nullptr;
  }
}

void Database::setError(const std::string& msg) {
  lastError_ = msg;
  // 마지막 개행 제거
  while (!lastError_.empty() && lastError_.back() == '\n') {
    lastError_.pop_back();
  }
}

bool Database::executeQuery(const std::string& sql) {
  if (!isConnected()) {
    setError("데이터베이스에 연결되지 않음");
    return false;
  }
  
  PGresult* res = PQexec(conn_, sql.c_str());
  ExecStatusType status = PQresultStatus(res);
  
  if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
    setError(PQerrorMessage(conn_));
    std::cerr << "SQL 실행 실패: " << lastError_ << std::endl;
    std::cerr << "SQL: " << sql << std::endl;
    PQclear(res);
    return false;
  }
  
  PQclear(res);
  return true;
}

std::vector<std::map<std::string, std::string>> Database::executeSelect(const std::string& sql) {
  std::vector<std::map<std::string, std::string>> results;
  
  if (!isConnected()) {
    setError("데이터베이스에 연결되지 않음");
    return results;
  }
  
  PGresult* res = PQexec(conn_, sql.c_str());
  
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    setError(PQerrorMessage(conn_));
    std::cerr << "SELECT 실패: " << lastError_ << std::endl;
    PQclear(res);
    return results;
  }
  
  int numRows = PQntuples(res);
  int numCols = PQnfields(res);
  
  // 컬럼 이름 가져오기
  std::vector<std::string> colNames;
  for (int col = 0; col < numCols; ++col) {
    colNames.push_back(PQfname(res, col));
  }
  
  // 각 행을 맵으로 변환
  for (int row = 0; row < numRows; ++row) {
    std::map<std::string, std::string> rowData;
    for (int col = 0; col < numCols; ++col) {
      if (PQgetisnull(res, row, col)) {
        rowData[colNames[col]] = "";  // NULL은 빈 문자열
      } else {
        rowData[colNames[col]] = PQgetvalue(res, row, col);
      }
    }
    results.push_back(rowData);
  }
  
  PQclear(res);
  return results;
}

} // namespace db_server
