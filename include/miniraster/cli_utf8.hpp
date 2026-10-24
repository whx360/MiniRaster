#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <cwchar>
#include <memory>
#endif

namespace mr {
inline std::vector<std::string> utf8_arguments(int argc,char* argv[]) {
    std::vector<std::string> result;
#ifdef _WIN32
    (void)argc; (void)argv;
    int count=0;
    wchar_t** wide=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!wide) throw std::runtime_error("cannot read Unicode command line");
    const std::unique_ptr<void,decltype(&LocalFree)> owner(wide,&LocalFree);
    result.reserve(static_cast<std::size_t>(count));
    for(int i=0;i<count;++i) {
        const int length=static_cast<int>(std::wcslen(wide[i]));
        if(length==0) { result.emplace_back();continue; }
        const int bytes=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],length,nullptr,0,nullptr,nullptr);
        if(bytes<=0) throw std::runtime_error("invalid Unicode argument");
        std::string value(static_cast<std::size_t>(bytes),'\0');
        if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide[i],length,value.data(),bytes,nullptr,nullptr)!=bytes)
            throw std::runtime_error("Unicode argument conversion failed");
        result.push_back(std::move(value));
    }
#else
    result.reserve(static_cast<std::size_t>(argc));
    for(int i=0;i<argc;++i) result.emplace_back(argv[i]);
#endif
    return result;
}
} // namespace mr
