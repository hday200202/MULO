#include <cstdlib>
#include <iostream>
#include <string>
#include <dlfcn.h>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include "PluginSandbox.hpp"

bool isLegitimateSystemPath(const char* pathname) {
    if (!pathname) return false;
    
    std::string path(pathname);
    
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
        cmd.find("python") != std::string::npos ||
        cmd.find("perl") != std::string::npos ||
        cmd.find("ruby") != std::string::npos ||
        cmd.find("node") != std::string::npos ||
        cmd.find("java") != std::string::npos) {
        return true;
    }
    
    return false;
}

extern "C" int system(const char* command) {
    // Only apply sandbox restrictions when sandboxing is active
    if (PluginSandbox::isSandboxActive() && containsMaliciousOperations(command)) {
        std::cout << "[SANDBOX] BLOCKED system() call with malicious operation for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << command << std::endl;
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
    // Always allow legitimate system paths (needed for audio system)
    if (isLegitimateSystemPath(pathname)) {
        static int (*real_open)(const char*, int, ...) = (int(*)(const char*, int, ...))dlsym(RTLD_NEXT, "open");
        if (real_open) {
            return real_open(pathname, flags);
        } else {
            errno = ENOSYS;
            return -1;
        }
    }
    
    // Only apply sandbox restrictions when sandboxing is active
    if (PluginSandbox::isSandboxActive() && (flags & (O_WRONLY | O_RDWR | O_CREAT | O_TRUNC | O_APPEND))) {
        std::cout << "[SANDBOX] BLOCKED open() with write flags for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_open)(const char*, int, ...) = (int(*)(const char*, int, ...))dlsym(RTLD_NEXT, "open");
    if (real_open) {
        return real_open(pathname, flags);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" int creat(const char* pathname, mode_t mode) {
    // Only apply sandbox restrictions when sandboxing is active
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
    // Always allow legitimate system paths (needed for audio system)
    if (isLegitimateSystemPath(pathname)) {
        static FILE* (*real_fopen)(const char*, const char*) = (FILE*(*)(const char*, const char*))dlsym(RTLD_NEXT, "fopen");
        if (real_fopen) {
            return real_fopen(pathname, mode);
        } else {
            errno = ENOSYS;
            return nullptr;
        }
    }
    
    // Only apply sandbox restrictions when sandboxing is active
    if (PluginSandbox::isSandboxActive() && mode && (strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+'))) {
        std::cout << "[SANDBOX] BLOCKED fopen() with write mode '" << mode 
                  << "' for plugin '" << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
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
    // Only apply sandbox restrictions when sandboxing is active
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
    // Always allow legitimate system paths (needed for audio system)
    if (isLegitimateSystemPath(pathname)) {
        static int (*real_mkdir)(const char*, mode_t) = (int(*)(const char*, mode_t))dlsym(RTLD_NEXT, "mkdir");
        if (real_mkdir) {
            return real_mkdir(pathname, mode);
        } else {
            errno = ENOSYS;
            return -1;
        }
    }
    
    // Only apply sandbox restrictions when sandboxing is active
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
    // Only apply sandbox restrictions when sandboxing is active
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

// ============================================================================
// NETWORK OPERATIONS BLOCKING
// ============================================================================

extern "C" int socket(int domain, int type, int protocol) {
    // Only apply sandbox restrictions when sandboxing is active
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED socket() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "' (domain=" << domain 
                  << ", type=" << type << ", protocol=" << protocol << ")" << std::endl;
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
    // Only apply sandbox restrictions when sandboxing is active
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
    // Only apply sandbox restrictions when sandboxing is active
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
    // Only apply sandbox restrictions when sandboxing is active
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
    // Only apply sandbox restrictions when sandboxing is active
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
    // Only apply sandbox restrictions when sandboxing is active
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
    // Only apply sandbox restrictions when sandboxing is active
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

// ============================================================================
// PROCESS EXECUTION BLOCKING
// ============================================================================

extern "C" int execve(const char* pathname, char* const argv[], char* const envp[]) {
    // Only apply sandbox restrictions when sandboxing is active
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

extern "C" int execl(const char* pathname, const char* arg, ...) {
    // Only apply sandbox restrictions when sandboxing is active
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED execl() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << pathname << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_execl)(const char*, const char*, ...) = 
        (int(*)(const char*, const char*, ...))dlsym(RTLD_NEXT, "execl");
    if (real_execl) {
        return real_execl(pathname, arg);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" int execlp(const char* file, const char* arg, ...) {
    // Only apply sandbox restrictions when sandboxing is active
    if (PluginSandbox::isSandboxActive()) {
        std::cout << "[SANDBOX] BLOCKED execlp() for plugin '" 
                  << PluginSandbox::getCurrentPlugin() << "': " << file << std::endl;
        errno = EACCES;
        return -1;
    }
    
    static int (*real_execlp)(const char*, const char*, ...) = 
        (int(*)(const char*, const char*, ...))dlsym(RTLD_NEXT, "execlp");
    if (real_execlp) {
        return real_execlp(file, arg);
    } else {
        errno = ENOSYS;
        return -1;
    }
}

extern "C" pid_t fork(void) {
    // Only apply sandbox restrictions when sandboxing is active
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
    // Only apply sandbox restrictions when sandboxing is active
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