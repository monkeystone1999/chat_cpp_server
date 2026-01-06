

class CryptManager {
private:
  CryptManager *instance = nullptr;
  CryptManager();

public:
  CryptManager *GetInstance() {
    if (instance == nullptr) {
    }
  }
}
