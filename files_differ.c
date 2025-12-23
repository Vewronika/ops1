int files_differ(const char *a, const char *b) {
    int fd_a = open(a, O_RDONLY);
    if (fd_a == -1) {
        return -1;
    }
    int fd_b = open(b, O_RDONLY);
    if (fd_b == -1) { 
        close(fd_a); 
        return -1; 
    }
    const size_t BUF = 64*1024;
    char *buf_a = malloc(BUF);
    char *buf_b = malloc(BUF);
    if (!buf_a || !buf_b) { 
        close(fd_a);
        close(fd_b); 
        free(buf_a); 
        free(buf_b); 
        return -1; 
    }

    int res = 0;
    while (1) {
        ssize_t ra = read(fd_a, buf_a, BUF);
        if (ra == -1 && errno == EINTR) {
            continue;
        }
        if (ra == -1) { 
            res = -1;
            break; 
        }
        ssize_t rb = read(fd_b, buf_b, BUF);
        if (rb == -1 && errno == EINTR){
            continue;
        } 
        if (rb == -1) { 
            res = -1; 
            break; 
        }
        if (ra == 0 && rb == 0) { 
            res = 0; 
            break; 
        } 
        if (ra != rb) {
            res = 1; 
            break; 
        }
        if (memcmp(buf_a, buf_b, (size_t)ra) != 0) { 
            res = 1;
            break;
        }
    }


    close(fd_a); 
    close(fd_b);
    free(buf_a); 
    free(buf_b);
    
    return res;
}