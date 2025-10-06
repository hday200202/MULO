#include <cstdlib>
#include <iostream>
#include <string>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <execinfo.h>
#include <stdarg.h>

#ifdef __APPLE__
    // macOS uses DYLD_INTERPOSE for function interposition
    #define DYLD_INTERPOSE(_replacement, _replacee) \
        __attribute__((used)) static struct { \
            const void* replacement; \
            const void* replacee; \
        } _interpose_##_replacee __attribute__((section("__DATA,__interpose"))) = { \
            (const void*)(unsigned long)&_replacement, \
            (const void*)(unsigned long)&_replacee \
        };
#else
    // Linux uses dlsym with RTLD_NEXT
    #include <dlfcn.h>
#endif

#include "PluginSandbox.hpp"

std::string getCallingPlugin() {
    void* callstack[8];
    int frames = backtrace(callstack, 8);
    char** strs = backtrace_symbols(callstack, frames);
    
    if (strs) {
        for (int i = 1; i < frames; i++) {
            std::string frame(strs[i]);
            
#ifdef __APPLE__
            // On macOS, look for .vst3 or .dylib in bundles
            size_t vst3Pos = frame.find(".vst3");
            size_t dylibPos = frame.find(".dylib");
            
            if (vst3Pos != std::string::npos || dylibPos != std::string::npos) {
                if (frame.find("/extensions/") != std::string::npos || 
                    frame.find("/VST3/") != std::string::npos) {
                    size_t start = frame.rfind('/', vst3Pos != std::string::npos ? vst3Pos : dylibPos);
                    if (start != std::string::npos) {
                        start++;
                        size_t end = vst3Pos != std::string::npos ? 
                                    frame.find(".vst3", start) + 5 : 
                                    frame.find(".dylib", start) + 6;
                        std::string pluginFile = frame.substr(start, end - start);
                        free(strs);
                        return pluginFile;
                    }
                }
            }
#else
            // On Linux, look for .so files
            size_t soPos = frame.find(".so");
            if (soPos != std::string::npos) {
                if (frame.find("/extensions/") != std::string::npos) {
                    size_t start = frame.rfind('/', soPos);
                    if (start != std::string::npos) {
                        start++;
                        size_t end = frame.find(".so", start) + 3;
                        std::string pluginFile = frame.substr(start, end - start);
                        free(strs);
                        return pluginFile;
                    }
                }
            }
#endif
        }
        free(strs);
    }
    
    return PluginSandbox::getCurrentPlugin();
}

bool isLegitimateSystemPath(const char* pathname) {
    if (!pathname) return false;
    
    std::string path(pathname);
    
#ifdef __APPLE__
    // macOS-specific system paths
    return (path.find("/dev/") != std::string::npos ||
            path.find("/private/tmp/") != std::string::npos ||
            path.find("/tmp/") != std::string::npos ||
            path.find("/var/folders/") != std::string::npos ||
            path.find("/System/Library/") != std::string::npos ||
            path.find("/Library/Audio/") != std::string::npos ||
            path.find("/Library/Preferences/Audio/") != std::string::npos ||
            path.find("CoreAudio") != std::string::npos ||
            path.find(".vst3") != std::string::npos ||
            path.find("config.json") != std::string::npos);
#else
    // Linux-specific system paths
    return (path.find("/dev/snd/") != std::string::npos ||
            path.find("/run/user/") != std::string::npos ||
            path.find("/tmp/.X11-unix/") != std::string::npos ||
            path.find("/tmp/.ICE-unix/") != std::string::npos ||
            path.find("/tmp/pulse-") != std::string::npos ||
            path.find("/proc/") != std::string::npos ||
            path.find("/sys/") != std::string::npos ||
            path.find("/usr/share/alsa/") != std::string::npos ||
            path.find("/etc/alsa/") != std::string::npos ||
            path.find("/var/lib/alsa/") != std::string::npos ||
            path.find(".vst3") != std::string::npos ||
            path.find("config.json") != std::string::npos);
#endif
}

bool containsFilesystemWrite(const char* command) {
    if (!command) return false;
    
    std::string cmd(command);
    
    return (cmd.find(">") != std::string::npos ||
            cmd.find(">>") != std::string::npos ||
            cmd.find("touch") != std::string::npos ||
            cmd.find("mkdir") != std::string::npos ||
            cmd.find("rm") != std::string::npos ||
            cmd.find("rmdir") != std::string::npos ||
            cmd.find("mv") != std::string::npos ||
            cmd.find("cp") != std::string::npos ||
            cmd.find("wget") != std::string::npos ||
            cmd.find("curl") != std::string::npos ||
            cmd.find("echo") != std::string::npos ||
            cmd.find("cat") != std::string::npos ||
            cmd.find("tee") != std::string::npos);
}

bool containsMaliciousOperations(const char* command) {
    if (!command) return false;
    
    std::string cmd(command);
    
    // Filesystem writes
    if (containsFilesystemWrite(command)) return true;
    
    // Network operations
    if (cmd.find("wget") != std::string::npos ||
        cmd.find("curl") != std::string::npos ||
        cmd.find("nc") != std::string::npos ||
        cmd.find("netcat") != std::string::npos ||
        cmd.find("telnet") != std::string::npos ||
        cmd.find("ssh") != std::string::npos ||
        cmd.find("scp") != std::string::npos ||
        cmd.find("rsync") != std::string::npos ||
        cmd.find("ftp") != std::string::npos ||
        cmd.find("sftp") != std::string::npos) {
        return true;
    }
    
    // Program execution and process manipulation
    if (cmd.find("exec") != std::string::npos ||
        cmd.find("eval") != std::string::npos ||
        cmd.find("source") != std::string::npos ||
        cmd.find("bash") != std::string::npos ||
        cmd.find("sh") != std::string::npos ||
#ifdef __APPLE__
        cmd.find("zsh") != std::string::npos ||
#endif
        cmd.find("python") != std::string::npos ||
        cmd.find("perl") != std::string::npos ||
        cmd.find("ruby") != std::string::npos ||
        cmd.find("node") != std::string::npos ||
        cmd.find("java") != std::string::npos) {
        return true;
    }
    
    return false;
}

// ============================================================================
// FUNCTION INTERPOSITION IMPLEMENTATIONS
// ============================================================================

#ifdef __APPLE__
// macOS: Use DYLD_INTERPOSE with wrapper functions
// We need to declare the real system functions that we'll call after interposition
extern "C" {
    int __real_system(const char*) __asm("_system");
    int __real_open(const char*, int, ...) __asm("_open");
    int __real_creat(const char*, mode_t) __asm("_creat");
    FILE* __real_fopen(const char*, const char*) __asm("_fopen");
    int __real_unlink(const char*) __asm("_unlink");
    int __real_mkdir(const char*, mode_t) __asm("_mkdir");
    int __real_rmdir(const char*) __asm("_rmdir");
    int __real_socket(int, int, int) __asm("_socket");
    int __real_connect(int, const struct sockaddr*, socklen_t) __asm("_connect");
    int __real_bind(int, const struct sockaddr*, socklen_t) __asm("_bind");
    int __real_listen(int, int) __asm("_listen");
    int __real_accept(int, struct sockaddr*, socklen_t*) __asm("_accept");
    ssize_t __real_sendto(int, const void*, size_t, int, const struct sockaddr*, socklen_t) __asm("_sendto");
    ssize_t __real_recvfrom(int, void*, size_t, int, struct sockaddr*, socklen_t*) __asm("_recvfrom");
    int __real_execve(const char*, char* const[], char* const[]) __asm("_execve");
    pid_t __real_fork(void) __asm("_fork");
    pid_t __real_vfork(void) __asm("_vfork");
}

int my_system(const char* command) {
    std::string callingPlugin = getCallingPlugin();
    bool pluginIsSandboxed = !callingPlugin.empty() && PluginSandbox::isPluginSandboxed(callingPlugin);
    if (pluginIsSandboxed && containsMaliciousOperations(command)) {
        std::cout << "[SANDBOX] BLOCKED system() for plugin '" 
                  << callingPlugin << "': " << command << std::endl;
        return -1;
    }
    
    return __real_system(command);
}

int my_open(const char* pathname, int flags, ...) {
    va_list args;
    va_start(args, flags);
    mode_t mode = (flags & O_CREAT) ? va_arg(args, int) : 0;
    va_end(args);
    
    if (isLegitimateSystemPath(pathname)) {
        return __real_open(pathname, flags, mode);
    }
    
    std::string callingPlugin = getCallingPlugin();
    bool pluginIsSandboxed = !callingPlugin.empty() && PluginSandbox::isPluginSandboxed(callingPlugin);
    if (pluginIsSandboxed && (flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND))) {
        std::cout << "[SANDBOX] BLOCKED open() with write flags for plugin '" 
                  << callingPlugin << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_open(pathname, flags, mode);
}

int my_creat(const char* pathname, mode_t mode) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED creat() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_creat(pathname, mode);
}

FILE* my_fopen(const char* pathname, const char* mode) {
    if (isLegitimateSystemPath(pathname)) {
        return __real_fopen(pathname, mode);
    }
    
    std::string callingPlugin = getCallingPlugin();
    bool pluginIsSandboxed = !callingPlugin.empty() && PluginSandbox::isPluginSandboxed(callingPlugin);
    
    if (pluginIsSandboxed && mode && (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+'))) {
        std::cout << "[SANDBOX] BLOCKED fopen() with write mode '" << mode 
                  << "' for plugin '" << callingPlugin << "': " << pathname << std::endl;
        errno = EACCES;
        return nullptr;
    }
    
    return __real_fopen(pathname, mode);
}

int my_unlink(const char* pathname) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED unlink() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_unlink(pathname);
}

int my_mkdir(const char* pathname, mode_t mode) {
    if (isLegitimateSystemPath(pathname)) {
        return __real_mkdir(pathname, mode);
    }
    
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED mkdir() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_mkdir(pathname, mode);
}

int my_rmdir(const char* pathname) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED rmdir() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_rmdir(pathname);
}

// Network operations
int my_socket(int domain, int type, int protocol) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED socket() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_socket(domain, type, protocol);
}

int my_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED connect() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_connect(sockfd, addr, addrlen);
}

int my_bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED bind() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_bind(sockfd, addr, addrlen);
}

int my_listen(int sockfd, int backlog) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED listen() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_listen(sockfd, backlog);
}

int my_accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED accept() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_accept(sockfd, addr, addrlen);
}

ssize_t my_sendto(int sockfd, const void* buf, size_t len, int flags,
                  const struct sockaddr* dest_addr, socklen_t addrlen) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED sendto() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}

ssize_t my_recvfrom(int sockfd, void* buf, size_t len, int flags,
                    struct sockaddr* src_addr, socklen_t* addrlen) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED recvfrom() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}

// Process execution
int my_execve(const char* pathname, char* const argv[], char* const envp[]) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED execve() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_execve(pathname, argv, envp);
}

pid_t my_fork(void) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED fork() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_fork();
}

pid_t my_vfork(void) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED vfork() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    return __real_vfork();
}

// DYLD_INTERPOSE declarations
DYLD_INTERPOSE(my_system, system)
DYLD_INTERPOSE(my_open, open)
DYLD_INTERPOSE(my_creat, creat)
DYLD_INTERPOSE(my_fopen, fopen)
DYLD_INTERPOSE(my_unlink, unlink)
DYLD_INTERPOSE(my_mkdir, mkdir)
DYLD_INTERPOSE(my_rmdir, rmdir)
DYLD_INTERPOSE(my_socket, socket)
DYLD_INTERPOSE(my_connect, connect)
DYLD_INTERPOSE(my_bind, bind)
DYLD_INTERPOSE(my_listen, listen)
DYLD_INTERPOSE(my_accept, accept)
DYLD_INTERPOSE(my_sendto, sendto)
DYLD_INTERPOSE(my_recvfrom, recvfrom)
DYLD_INTERPOSE(my_execve, execve)
DYLD_INTERPOSE(my_fork, fork)
DYLD_INTERPOSE(my_vfork, vfork)

#else
// Linux: Use extern "C" with dlsym

extern "C" int system(const char* command) {
    std::string callingPlugin = getCallingPlugin();
    bool pluginIsSandboxed = !callingPlugin.empty() && PluginSandbox::isPluginSandboxed(callingPlugin);
    if (pluginIsSandboxed && containsMaliciousOperations(command)) {
        std::cout << "[SANDBOX] BLOCKED system() for plugin '" 
                  << callingPlugin << "': " << command << std::endl;
        return -1;
    }
    
    static int (*real_system)(const char*) = (int(*)(const char*))dlsym(RTLD_NEXT, "system");
    if (real_system) {
        return real_system(command);
    } else {
        return -1;
    }
}

extern "C" int open(const char* pathname, int flags, ...) {
    va_list args;
    va_start(args, flags);
    mode_t mode = (flags & O_CREAT) ? va_arg(args, int) : 0;
    va_end(args);
    
    if (isLegitimateSystemPath(pathname)) {
        static int (*real_open)(const char*, int, mode_t) = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");
        if (real_open) {
            return real_open(pathname, flags, mode);
        } else {
            errno = ENOSYS;
            return -1;
        }
    }
    
    std::string callingPlugin = getCallingPlugin();
    bool pluginIsSandboxed = !callingPlugin.empty() && PluginSandbox::isPluginSandboxed(callingPlugin);
    if (pluginIsSandboxed && (flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND))) {
        std::cout << "[SANDBOX] BLOCKED open() with write flags for plugin '" 
                  << callingPlugin << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_open)(const char*, int, mode_t) = (int(*)(const char*, int, mode_t))dlsym(RTLD_NEXT, "open");
    if (real_open) {
        return real_open(pathname, flags, mode);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" int creat(const char* pathname, mode_t mode) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED creat() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_creat)(const char*, mode_t) = (int(*)(const char*, mode_t))dlsym(RTLD_NEXT, "creat");
    if (real_creat) {
        return real_creat(pathname, mode);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" FILE* fopen(const char* pathname, const char* mode) {
    if (isLegitimateSystemPath(pathname)) {
        static FILE* (*real_fopen)(const char*, const char*) = (FILE*(*)(const char*, const char*))dlsym(RTLD_NEXT, "fopen");
        if (real_fopen) {
            return real_fopen(pathname, mode);
        } else {
            errno = ENOSYS;
            return nullptr;
        }
    }
    
    std::string callingPlugin = getCallingPlugin();
    bool pluginIsSandboxed = !callingPlugin.empty() && PluginSandbox::isPluginSandboxed(callingPlugin);
    
    if (pluginIsSandboxed && mode && (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+'))) {
        std::cout << "[SANDBOX] BLOCKED fopen() with write mode '" << mode 
                  << "' for plugin '" << callingPlugin << "': " << pathname << std::endl;
        errno = EACCES;
        return nullptr;
    }
    
    static FILE* (*real_fopen)(const char*, const char*) = (FILE*(*)(const char*, const char*))dlsym(RTLD_NEXT, "fopen");
    if (real_fopen) {
        return real_fopen(pathname, mode);
    } else {
        errno = ENOSYS;
        return nullptr;
    }
}

extern "C" int unlink(const char* pathname) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED unlink() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_unlink)(const char*) = (int(*)(const char*))dlsym(RTLD_NEXT, "unlink");
    if (real_unlink) {
        return real_unlink(pathname);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" int mkdir(const char* pathname, mode_t mode) {
    if (isLegitimateSystemPath(pathname)) {
        static int (*real_mkdir)(const char*, mode_t) = (int(*)(const char*, mode_t))dlsym(RTLD_NEXT, "mkdir");
        if (real_mkdir) {
            return real_mkdir(pathname, mode);
        } else {
            errno = ENOSYS;
            return -1;
        }
    }
    
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED mkdir() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_mkdir)(const char*, mode_t) = (int(*)(const char*, mode_t))dlsym(RTLD_NEXT, "mkdir");
    if (real_mkdir) {
        return real_mkdir(pathname, mode);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" int rmdir(const char* pathname) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED rmdir() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_rmdir)(const char*) = (int(*)(const char*))dlsym(RTLD_NEXT, "rmdir");
    if (real_rmdir) {
        return real_rmdir(pathname);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" int socket(int domain, int type, int protocol) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED socket() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_socket)(int, int, int) = (int(*)(int, int, int))dlsym(RTLD_NEXT, "socket");
    if (real_socket) {
        return real_socket(domain, type, protocol);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED connect() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_connect)(int, const struct sockaddr*, socklen_t) = 
        (int(*)(int, const struct sockaddr*, socklen_t))dlsym(RTLD_NEXT, "connect");
    if (real_connect) {
        return real_connect(sockfd, addr, addrlen);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED bind() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_bind)(int, const struct sockaddr*, socklen_t) = 
        (int(*)(int, const struct sockaddr*, socklen_t))dlsym(RTLD_NEXT, "bind");
    if (real_bind) {
        return real_bind(sockfd, addr, addrlen);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" int listen(int sockfd, int backlog) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED listen() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_listen)(int, int) = (int(*)(int, int))dlsym(RTLD_NEXT, "listen");
    if (real_listen) {
        return real_listen(sockfd, backlog);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED accept() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_accept)(int, struct sockaddr*, socklen_t*) = 
        (int(*)(int, struct sockaddr*, socklen_t*))dlsym(RTLD_NEXT, "accept");
    if (real_accept) {
        return real_accept(sockfd, addr, addrlen);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" ssize_t sendto(int sockfd, const void* buf, size_t len, int flags,
                          const struct sockaddr* dest_addr, socklen_t addrlen) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED sendto() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static ssize_t (*real_sendto)(int, const void*, size_t, int, const struct sockaddr*, socklen_t) = 
        (ssize_t(*)(int, const void*, size_t, int, const struct sockaddr*, socklen_t))dlsym(RTLD_NEXT, "sendto");
    if (real_sendto) {
        return real_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" ssize_t recvfrom(int sockfd, void* buf, size_t len, int flags,
                            struct sockaddr* src_addr, socklen_t* addrlen) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED recvfrom() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static ssize_t (*real_recvfrom)(int, void*, size_t, int, struct sockaddr*, socklen_t*) = 
        (ssize_t(*)(int, void*, size_t, int, struct sockaddr*, socklen_t*))dlsym(RTLD_NEXT, "recvfrom");
    if (real_recvfrom) {
        return real_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" int execve(const char* pathname, char* const argv[], char* const envp[]) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED execve() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_execve)(const char*, char* const[], char* const[]) = 
        (int(*)(const char*, char* const[], char* const[]))dlsym(RTLD_NEXT, "execve");
    if (real_execve) {
        return real_execve(pathname, argv, envp);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" pid_t fork(void) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED fork() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static pid_t (*real_fork)(void) = (pid_t(*)(void))dlsym(RTLD_NEXT, "fork");
    if (real_fork) {
        return real_fork();
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" pid_t vfork(void) {
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED vfork() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "'" << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static pid_t (*real_vfork)(void) = (pid_t(*)(void))dlsym(RTLD_NEXT, "vfork");
    if (real_vfork) {
        return real_vfork();
    } else {
        errno = ENOSYS;
        return -1;
    }
}

#endif // __APPLE__
