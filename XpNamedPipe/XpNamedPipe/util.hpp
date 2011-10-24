#pragma once

#include <stdexcept>
#include <string>
#include <sstream>

namespace util {
    static std::string getWindowsErrorMessage(const std::string& prepend, const std::string& funcName) {
        DWORD error = GetLastError();

        std::stringstream msg;
        if (!prepend.empty()) {
            msg << prepend << ": ";
        }

        char buffer[1024] = "";

        if (FormatMessageA(
                FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                error,
                0,
                buffer, sizeof(buffer), NULL) == 0) {
            msg << "Failed to retrieve Windows error message";
        } else {
            msg << funcName << " failed with " << error << ": " << buffer;
        }
        return msg.str();
    }

    static std::string getWindowsErrorMessage(const std::string& funcName) {
        return getWindowsErrorMessage(std::string(), funcName);
    }

    static void throwWindowsError(const std::string& prepend, const std::string& funcName) {
        throw std::runtime_error(getWindowsErrorMessage(prepend, funcName));
    }

    static void throwWindowsError(const std::string& funcName) {
        throwWindowsError(std::string(), funcName);
    }

    static void checkWindowsResult(BOOL result, const std::string& prepend, const std::string& funcName) {
        if (!result) {
            throwWindowsError(prepend, funcName);
        }
    }

    static void checkWindowsResult(BOOL result, const std::string& funcName) {
        checkWindowsResult(result, std::string(), funcName);
    }

    template <HANDLE INVALID_HANDLE> class ScopedHandleT
    {
    public:
        ScopedHandleT() {
            this->handle = INVALID_HANDLE;
        }

        ScopedHandleT(HANDLE handle) {
            this->handle = handle;
        }

        ScopedHandleT& operator = (HANDLE otherHandle) {
            if (this->handle != INVALID_HANDLE) {
                CloseHandle(this->handle);
            }
            this->handle = otherHandle;
            return *this;
        }

        virtual ~ScopedHandleT() {
            if (handle != INVALID_HANDLE) {
                CloseHandle(handle);
            }
        }

        operator HANDLE () const {
            return handle;
        }

        void check(const std::string& prepend, const std::string& funcName) {
            if (handle == INVALID_HANDLE) {
                throwWindowsError(prepend, funcName);
            }
        }

        void check(const std::string& funcName) {
            check(std::string(), funcName);
        }

    private:
        HANDLE handle;
    };

    typedef ScopedHandleT<NULL> ScopedHandle;
    typedef ScopedHandleT<INVALID_HANDLE_VALUE> ScopedFileHandle;

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
        } catch (...) {
            if (result != NULL) {
                delete [] result;
            }
            throw;
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
        } catch (...) {
            if (result != NULL) {
                delete [] result;
            }
            throw;
        }
        return result;
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

}