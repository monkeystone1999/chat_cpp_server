#pragma once

#include <libpq-fe.h>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <functional>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace db_server {

// ============================================================================
// 필드 정보 구조체 (리플렉션용)
// ============================================================================
struct FieldInfo {
  std::string name;       // 필드 이름
  std::string typeName;   // 타입 이름 (TEXT, INTEGER, REAL 등)
  std::string value;      // 문자열로 변환된 값
};

// ============================================================================
// 구조체 리플렉션 매크로
// 사용법:
//   struct User {
//     int id;
//     std::string name;
//     double score;
//     
//     DB_FIELDS(User,
//       DB_FIELD(id),
//       DB_FIELD(name),
//       DB_FIELD(score)
//     )
//   };
// ============================================================================

// 타입을 PostgreSQL 타입 문자열로 변환
template<typename T>
struct PgType { static constexpr const char* name = "TEXT"; };

template<> struct PgType<int> { static constexpr const char* name = "INTEGER"; };
template<> struct PgType<long> { static constexpr const char* name = "BIGINT"; };
template<> struct PgType<long long> { static constexpr const char* name = "BIGINT"; };
template<> struct PgType<float> { static constexpr const char* name = "REAL"; };
template<> struct PgType<double> { static constexpr const char* name = "DOUBLE PRECISION"; };
template<> struct PgType<bool> { static constexpr const char* name = "BOOLEAN"; };
template<> struct PgType<std::string> { static constexpr const char* name = "TEXT"; };

// 값을 문자열로 변환
template<typename T>
std::string toDbString(const T& val) {
  if constexpr (std::is_same_v<T, std::string>) {
    return "'" + val + "'";  // SQL 문자열은 따옴표로 감싸기
  } else if constexpr (std::is_same_v<T, bool>) {
    return val ? "TRUE" : "FALSE";
  } else if constexpr (std::is_arithmetic_v<T>) {
    return std::to_string(val);
  } else {
    std::ostringstream oss;
    oss << val;
    return "'" + oss.str() + "'";
  }
}

// 구조체 필드 등록 매크로
#define DB_FIELD(field) \
  fields.push_back({ \
    #field, \
    db_server::PgType<decltype(field)>::name, \
    db_server::toDbString(field) \
  })

#define DB_FIELDS(StructName, ...) \
  std::vector<db_server::FieldInfo> getFields() const { \
    std::vector<db_server::FieldInfo> fields; \
    __VA_ARGS__; \
    return fields; \
  } \
  static std::string tableName() { return #StructName; }

// ============================================================================
// PostgreSQL 연결 클래스 (SSL 지원)
// ============================================================================
class Database {
public:
  // 생성자: SSL 인증서 기반 연결
  // @param host: 호스트 주소 (Docker 컨테이너 IP 또는 이름)
  // @param port: 포트 (기본 5432)
  // @param dbname: 데이터베이스 이름
  // @param user: 사용자 이름
  // @param password: 비밀번호
  // @param sslCert: 클라이언트 인증서 경로
  // @param sslKey: 클라이언트 개인키 경로
  // @param sslRootCert: CA 인증서 경로
  Database(const std::string& host, int port, const std::string& dbname,
           const std::string& user, const std::string& password,
           const std::string& sslCert = "", const std::string& sslKey = "",
           const std::string& sslRootCert = "");
  
  ~Database();
  
  // 복사 금지
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  
  // 연결 상태 확인
  bool isConnected() const;
  
  // 연결 종료
  void disconnect();

  // ============================================================================
  // CRUD 함수 (구조체 기반)
  // ============================================================================
  
  // CREATE: 구조체를 테이블에 삽입
  // @param obj: DB_FIELDS 매크로가 정의된 구조체
  // @return: 성공 시 true
  template<typename T>
  bool insert(const T& obj) {
    auto fields = obj.getFields();
    std::string tableName = T::tableName();
    
    std::ostringstream columns, values;
    for (size_t i = 0; i < fields.size(); ++i) {
      if (i > 0) { columns << ", "; values << ", "; }
      columns << fields[i].name;
      values << fields[i].value;
    }
    
    std::string sql = "INSERT INTO " + tableName + " (" + columns.str() + ") VALUES (" + values.str() + ")";
    return executeQuery(sql);
  }
  
  // READ: 조건에 맞는 데이터 조회
  // @param whereClause: WHERE 절 (예: "id = 1")
  // @return: 결과 행들 (각 행은 컬럼명->값 맵)
  template<typename T>
  std::vector<std::map<std::string, std::string>> select(const std::string& whereClause = "") {
    std::string tableName = T::tableName();
    std::string sql = "SELECT * FROM " + tableName;
    if (!whereClause.empty()) {
      sql += " WHERE " + whereClause;
    }
    return executeSelect(sql);
  }
  
  // UPDATE: 구조체 값으로 업데이트
  // @param obj: 업데이트할 값이 담긴 구조체
  // @param whereClause: WHERE 절 (필수)
  // @return: 성공 시 true
  template<typename T>
  bool update(const T& obj, const std::string& whereClause) {
    auto fields = obj.getFields();
    std::string tableName = T::tableName();
    
    std::ostringstream setClause;
    for (size_t i = 0; i < fields.size(); ++i) {
      if (i > 0) setClause << ", ";
      setClause << fields[i].name << " = " << fields[i].value;
    }
    
    std::string sql = "UPDATE " + tableName + " SET " + setClause.str() + " WHERE " + whereClause;
    return executeQuery(sql);
  }
  
  // DELETE: 조건에 맞는 데이터 삭제
  // @param whereClause: WHERE 절 (필수)
  // @return: 성공 시 true
  template<typename T>
  bool remove(const std::string& whereClause) {
    std::string tableName = T::tableName();
    std::string sql = "DELETE FROM " + tableName + " WHERE " + whereClause;
    return executeQuery(sql);
  }
  
  // CREATE TABLE: 구조체 기반 테이블 생성
  template<typename T>
  bool createTable() {
    T dummy{};
    auto fields = dummy.getFields();
    std::string tableName = T::tableName();
    
    std::ostringstream columns;
    for (size_t i = 0; i < fields.size(); ++i) {
      if (i > 0) columns << ", ";
      columns << fields[i].name << " " << fields[i].typeName;
    }
    
    std::string sql = "CREATE TABLE IF NOT EXISTS " + tableName + " (" + columns.str() + ")";
    return executeQuery(sql);
  }
  
  // 직접 SQL 실행
  bool executeQuery(const std::string& sql);
  std::vector<std::map<std::string, std::string>> executeSelect(const std::string& sql);
  
  // 마지막 에러 메시지
  std::string getLastError() const { return lastError_; }

private:
  PGconn* conn_;
  std::string lastError_;
  
  void setError(const std::string& msg);
};

} // namespace db_server
