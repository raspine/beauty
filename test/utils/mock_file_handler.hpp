#pragma once
#include <stdint.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "i_file_handler.hpp"
#include "reply.hpp"

class MockFileHandler : public http::server::IFileHandler {
   public:
    MockFileHandler() = default;
    virtual ~MockFileHandler() = default;

    virtual size_t openFileForRead(unsigned id, const std::string& path) override;
    virtual void closeFile(unsigned id) override;
    virtual int readFile(unsigned id, char* buf, size_t maxSize) override;
    virtual http::server::Reply::status_type openFileForWrite(unsigned id,
                                                              const std::string& path,
                                                              std::string& err) override;
    virtual http::server::Reply::status_type writeFile(unsigned id,
                                                       const char* buf,
                                                       size_t size,
                                                       std::string& err) override;

    void createMockFile(uint32_t size);
    void setMockFailToOpenRequestedFile();

    int getOpenFileCalls();
    int getReadFileCalls();
    int getCloseFileCalls();

   private:
    struct OpenFile {
        std::vector<char>::iterator readIt_;
        bool isOpen_ = false;
    };
    std::unordered_map<unsigned, OpenFile> openFiles_;
    std::vector<char> mockFileData_;
    int countOpenFileCalls_ = 0;
    int countReadFileCalls_ = 0;
    int countCloseFileCalls_ = 0;
    bool mockFailToOpenRequestedFile_ = false;
};

