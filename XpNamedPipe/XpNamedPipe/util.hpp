#pragma once

namespace util {
    std::string getWindowsErrorMessage(const std::string& prepend, const std::string& funcName) {
        const int BUF_LEN = 1024;

        DWORD error = GetLastError();
        char msg[BUF_LEN] = "";
        char buffer[BUF_LEN] = "";
        if (FormatMessageA(
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                error,
                0,
                buffer, sizeof(buffer), NULL) == 0) {
            sprintf_s(msg, "Failed to retrieve Windows error message");       
        } else {
            sprintf_s(msg, "%s failed with %lu: %s", funcName, error, buffer);
        }
        if (!prepend.empty()) {
            return prepend + ": " + std::string(msg);
        } 
        return std::string(msg);
    }

    std::string getWindowsErrorMessage(const std::string& funcName) {
        return getWindowsErrorMessage(std::string(), funcName);
    }

    void throwWindowsError(const std::string& prepend, const std::string& funcName) {
        throw std::runtime_error(getWindowsErrorMessage(prepend, funcName));
    }

    void throwWindowsError(const std::string& funcName) {
        throwWindowsError(std::string(), funcName);
    }

    template <HANDLE INVALID_HANDLE> class ScopedHandleT
    {
    public:
        ScopedHandleT(HANDLE handle) {
            this->handle = handle;
        }

        virtual ~ScopedHandleT() {
            if (handle != INVALID_HANDLE) {
                CloseHandle(handle);
            }
        }

        operator HANDLE () const {
            return handle;
        }
    private:
        HANDLE handle;
    };

    typedef ScopedHandleT<NULL> ScopedHandle;
    typedef ScopedHandleT<INVALID_HANDLE_VALUE> ScopedFileHandle;

    class xstring : public std::string {
    public:
        xstring(const std::wstring& from) : std::string(toUtf8(from)) {
        }

        std::wstring utf16() const {
            return toUtf16(*this);
        }

        static std::string toUtf8(const std::wstring& utf16) {
            char* utf8 = newUtf8(utf16.c_str());
            std::string result = utf8;
            delete [] utf8;
            return result;
        }

        static std::wstring toUtf16(const std::string& utf8) {
            wchar_t* utf16 = newUtf16(utf8.c_str());
            std::wstring result = utf16;
            delete [] utf16;
            return result;
        }

        static char* newUtf8(const wchar_t* utf16) {
            char* result = NULL;
            try {
                int utf8BufLen = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, NULL, 0, NULL, NULL);
                if (utf8BufLen == 0) {
                    throwWindowsError("Error converting string to UTF-8: ", "WideCharToMultiByte");
                }
                result = new char[utf8BufLen];
                int conversionResult = WideCharToMultiByte(CP_UTF8, 0, utf16, -1, result, utf8BufLen, NULL, NULL);
                if (conversionResult == 0) {
                    throwWindowsError("Error converting string to UTF-8: ", "WideCharToMultiByte");
                }
            } catch (std::exception& e) {
                if (result != NULL) {
                    delete [] result;
                }
                throw e;
            }
            return result;
        }

        static wchar_t* newUtf16(const char* utf8) {
            wchar_t* result = NULL;
            try {
                int utf16BufLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
                if (utf16BufLen == 0) {
                    throwWindowsError("Error converting string to UTF-16: ", "MultiByteToWideChar");
                }
                result = new wchar_t[utf16BufLen];
                int conversionResult = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, result, utf16BufLen);
                if (conversionResult == 0) {
                    throwWindowsError("Error converting string to UTF-16: ", "MultiByteToWideChar");
                }
            } catch (std::exception& e) {
                if (result != NULL) {
                    delete [] result;
                }
                throw e;
            }
            return result;
        }

    };

}