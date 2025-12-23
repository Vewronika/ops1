#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
//#include "parser.h"
#include <signal.h>
#include <sys/inotify.h>
#include <limits.h>

#define MAX_WATCHES 8192
#define EVENT_BUF_LEN (64 * (sizeof(struct inotify_event) + NAME_MAX + 1))

struct Watch {
    int wd; // key
    char *path; // value
};

struct WatchMap {
    struct Watch watch_map[MAX_WATCHES];
    int watch_count;
};


volatile sig_atomic_t terminate = 0;
 
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))

uint32_t pending_cookie = 0;
char pending_old_path[PATH_MAX] = {0};


//#define PATH_MAX 4096


void sethandler(void (*f)(int), int sigNo)
{
    struct sigaction act;
    memset(&act, 0, sizeof(struct sigaction));
    act.sa_handler = f;

    if (-1 == sigaction(sigNo, &act, NULL))
        perror("sigaction"); 
}

void term_handler(int sigNo){
    //sethandler(SIG_IGN, SIGTERM);
    terminate = 1;
}

enum cmd_type { CMD_INVALID=0, CMD_ADD, CMD_END, CMD_LIST, CMD_RESTORE, CMD_EXIT };

struct backup_entry { 
    char *src_resolved; 
    char *dst_resolved; 
    pid_t pid;
};

struct parse_result {
    enum cmd_type cmd;
    char *src, **dst; 
    char *src_resolved, **dst_resolved;
    int src_is_symlink;
    int id;
    char *error;
    int num;
};

static void set_error(struct parse_result *r, const char *msg)
{
    free(r->error);
    r->error = strdup(msg);
}

void add_to_map(struct WatchMap *map, int wd, const char *path) {
    if (map->watch_count >= MAX_WATCHES) {
        fprintf(stderr, "Exceeded max watches!\n");
        return;
    }
    map->watch_map[map->watch_count].wd = wd;
    map->watch_map[map->watch_count].path = strdup(path); // Must copy the path!
    map->watch_count++;
    //printf("new watch: '%s' @wd=%d\n", path, wd);
}


void free_watch_map(struct WatchMap *map)
{
    for (int i = 0; i < map->watch_count; i++) {
        free(map->watch_map[i].path);
    }
    map->watch_count = 0;
}



void add_watch_recursive(int notify_fd, struct WatchMap *map, const char *base_path) {
    uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY |
                    IN_MOVED_FROM | IN_MOVED_TO;

    int wd = inotify_add_watch(notify_fd, base_path, mask);
    if (wd < 0) {
        perror("inotify_add_watch");
        return;
    }
    add_to_map(map, wd, base_path);

    DIR *dir = opendir(base_path);
    if (!dir) { 
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            add_watch_recursive(notify_fd, map, full_path);
        }
    }

    closedir(dir);
}

struct Watch *find_watch(struct WatchMap *map, int wd) {
    for (int i = 0; i < map->watch_count; i++) {
        if (map->watch_map[i].wd == wd) {
            return &map->watch_map[i];
        }
    }
    return NULL;
}

void remove_from_map(struct WatchMap *map, int wd) {
    for (int i = 0; i < map->watch_count; i++) {
        if (map->watch_map[i].wd == wd) {
            printf("removing watch: '%s' @wd=%d\n", map->watch_map[i].path, wd);
            free(map->watch_map[i].path);
            map->watch_map[i] = map->watch_map[map->watch_count - 1];
            map->watch_count--;
            return;
        }
    }
}

void update_watch_paths(struct WatchMap *map, const char *old_path, const char *new_path) {
    size_t old_len = strlen(old_path);

    for (int i = 0; i < map->watch_count; i++) {
        if (strncmp(map->watch_map[i].path, old_path, old_len) == 0 && (
                map->watch_map[i].path[old_len] == '/' || map->watch_map[i].path[old_len] == '\0')) {
            char new_full_path[PATH_MAX];

            snprintf(new_full_path, sizeof(new_full_path), "%s%s",
                     new_path, map->watch_map[i].path + old_len);

            free(map->watch_map[i].path);
            map->watch_map[i].path = strdup(new_full_path);
        }
    }
}






void remove_backup(struct backup_entry *backs, int *n, pid_t pid) {

    for (int i = 0; i < *n; ++i) {
        if (backs[i].pid == pid) {
            free(backs[i].src_resolved);
            free(backs[i].dst_resolved);
            backs[i] = backs[*n - 1];
            (*n)--;
            return;
        }
    }
}


void reap_children(struct backup_entry *backs, int *n) {
    int status;
    pid_t w;
    while ((w = waitpid(-1, &status, WNOHANG)) > 0) {
        remove_backup(backs, n, w);
    }
}


static int tokenize(const char *s, char ***out, int *out_n, char **err) {
    char **toks = NULL; int n = 0, cap = 0;
    const char *p = s;

    while (*p) {
        while (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r') p++;
        if (!*p) break;

        char buf[PATH_MAX]; int bi = 0;

        if (*p=='\'' || *p=='"') {
            char q = *p++;
            while (*p && *p!=q) {
                if (*p=='\\' && p[1]) {
                    p++; if (bi < PATH_MAX-1) buf[bi++] = *p++;
                } else {
                    if (bi < PATH_MAX-1) buf[bi++] = *p++;
                    else p++;
                }
            }
            if (!*p) goto unmatched;
            p++;
        } else {
            while (*p && *p!=' ' && *p!='\t' && *p!='\n' && *p!='\r') {
                if (*p=='\\' && p[1]) {
                    p++; if (bi < PATH_MAX-1) buf[bi++] = *p++;
                } else if (*p=='\'' || *p=='"') {
                    char q = *p++;
                    while (*p && *p!=q) {
                        if (*p=='\\' && p[1]) {
                            p++; if (bi < PATH_MAX-1) buf[bi++] = *p++;
                        } else {
                            if (bi < PATH_MAX-1) buf[bi++] = *p++;
                            else p++;
                        }
                    }
                    if (!*p) goto unmatched;
                    p++;
                } else {
                    if (bi < PATH_MAX-1) buf[bi++] = *p++;
                    else p++;
                }
            }
        }

        buf[bi] = '\0';

        if (n+1 > cap) {
            cap = cap ? cap*2 : 8;
            char **tmp = realloc(toks, cap*sizeof(char*));
            if (!tmp) goto oom;
            toks = tmp;
        }
        toks[n++] = strdup(buf);
    }

    *out = toks; *out_n = n; *err = NULL;
    return 0;

unmatched:
    *err = strdup("unmatched quote");
    goto fail;
oom:
    *err = strdup("out of memory");
fail:
    for (int i=0;i<n;i++) free(toks[i]);
    free(toks);
    return -1;
}




static int check_path(const char *in, char *out, size_t olen) {

    struct stat st;
    if (lstat(in, &st) == 0)
        return realpath(in, out) ? 0 : -1;

    char tmp[PATH_MAX];
    if (strlen(in) >= sizeof(tmp)) return -1;

    strcpy(tmp, in);

    char *slash = strrchr(tmp, '/');
    char parent[PATH_MAX], base[PATH_MAX];

    if (!slash) {
        if (!getcwd(parent, sizeof(parent))) return -1;
        strncpy(base, tmp, sizeof(base)); 
        base[sizeof(base)-1] = '\0';
    } 
    else if (slash == tmp) {
        strcpy(parent, "/");
        strncpy(base, slash+1, sizeof(base));
        base[sizeof(base)-1] = '\0';
    } else {
        *slash = '\0';
        strncpy(parent, tmp, sizeof(parent)); parent[sizeof(parent)-1] = '\0';
        strncpy(base, slash+1, sizeof(base)); base[sizeof(base)-1] = '\0';
    }

    char pr[PATH_MAX];
    if (!realpath(parent, pr)) return -1;
    return snprintf(out, olen, "%s/%s", pr, base) < (int)olen ? 0 : -1;
}



static int path_is_prefix(const char *p, const char *c) {
    size_t n = strlen(p);
    if (!strcmp(p, "/")) return 1;
    return !strncmp(p, c, n) && (c[n]=='\0' || c[n]=='/');
}



static int is_duplicate_pair(const struct backup_entry *e, int n,
                              const char *s, const char *d) {
    for (int i=0;i<n;i++)
        if (e[i].src_resolved && e[i].dst_resolved &&
            !strcmp(e[i].src_resolved, s) && !strcmp(e[i].dst_resolved, d))
            return 1;
    return 0;
}





int parse_input(const char *line, const struct backup_entry *existing, int existing_n, struct parse_result *r) {
    
    memset(r, 0, sizeof(*r));

    char **toks=NULL, *terr=NULL; 
    int tc=0;

    if (tokenize(line, &toks, &tc, &terr) < 0) {
        r->error = terr;
        return -1;
    }

    if (tc==0) { 
        //printf("empty\n");
        goto out; 
    }

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "%s", toks[0]);
    for (char *c=cmd;*c;c++) if (*c>='A'&&*c<='Z') *c += 'a'-'A';

    if (!strcmp(cmd,"add")) {
        if (tc<3) {
            r->error=strdup("usage: add <src> <dst>");
            goto out;
        }

        r->src=strdup(toks[1]);

        r->num = tc-2;

        r->dst = malloc((tc-2)*sizeof(char*));
                r->dst_resolved = malloc((tc-2)*sizeof(char*));


        for(int i =0; i < tc-2; i++){
            r->dst[i]=strdup(toks[2+i]);
        }





        struct stat st;
        if (lstat(r->src,&st)!=0) { 
            set_error(r,"src invalid");
            goto out; }
        r->src_is_symlink = S_ISLNK(st.st_mode);

        char buf[PATH_MAX];
        if (!realpath(r->src, buf)) { 
            set_error(r,"src resolve");
            goto out;
        }

        r->src_resolved=strdup(buf);

        for(int i = 0; i <tc-2; i++){
            if (check_path(r->dst[i], buf, sizeof(buf))!=0) { 
                set_error(r,"dst invalid");
                goto out; 
            }
            r->dst_resolved[i]=strdup(buf);
        }

        for(int i=0; i < tc-2; i++){

        
            if (!strcmp(r->src_resolved, r->dst_resolved[i]) || path_is_prefix(r->src_resolved, r->dst_resolved[i]) || path_is_prefix(r->dst_resolved[i], r->src_resolved) ||
                (existing && is_duplicate_pair(existing, existing_n, r->src_resolved, r->dst_resolved[i]))) {

                set_error(r,"invalid src/dst pair");
                goto out;
            }
        }

        r->cmd = CMD_ADD;

    } 
    else if (!strcmp(cmd,"end")) {
        if (tc!=3) {
            r->error=strdup("usage: end <src> <dst>");
            goto out;
        }

        r->dst = malloc(sizeof(char*));
        r->dst_resolved = malloc(sizeof(char*));
        r->num = 1;

        r->src = strdup(toks[1]);
        r->dst[0] = strdup(toks[2]);

        char buf[PATH_MAX];
        if (!realpath(r->src, buf)) { 
            set_error(r,"src resolve");
            goto out;
        }

        r->src_resolved=strdup(buf);

            if (check_path(r->dst[0], buf, sizeof(buf))!=0) { 
                set_error(r,"dst invalid");
                goto out; 
            }
            r->dst_resolved[0]=strdup(buf);
        
        
        r->cmd = CMD_END;

    } else if (!strcmp(cmd,"list")) {
        if (tc!=1) { r->error=strdup("usage: list"); goto out; }
        r->cmd = CMD_LIST;

    } 
    else if (!strcmp(cmd,"restore")) {
        if (tc!=3) { r->error=strdup("usage: restore <src> <dst>"); goto out; }

        r->dst = malloc(sizeof(char*));
        r->dst_resolved = malloc(sizeof(char*));
        r->num = 1;

        r->src = strdup(toks[1]);
        r->dst[0] = strdup(toks[2]);

        char buf[PATH_MAX];
        if (!realpath(r->src, buf)) { 
            set_error(r,"src resolve");
            goto out;
        }

        r->src_resolved=strdup(buf);

            if (check_path(r->dst[0], buf, sizeof(buf))!=0) { 
                set_error(r,"dst invalid");
                goto out; 
            }
            r->dst_resolved[0]=strdup(buf);
        r->cmd = CMD_RESTORE;

    }
    else if (!strcmp(cmd,"exit")) {
        if (tc!=1) { 
            r->error=strdup("usage: exit"); 
            goto out; }
        r->cmd = CMD_EXIT;
    } else {
        set_error(r,"unknown command");
    }

out:
    for (int i=0;i<tc;i++) free(toks[i]);
    free(toks);
    return r->error ? -1 : 0;
}

void free_parse_result(struct parse_result *r)
{
    if (r->src) free(r->src);

    if (r->dst) {
        for (int i = 0; i < r->num; i++)
            free(r->dst[i]);
        free(r->dst);
    }

    if (r->src_resolved) free(r->src_resolved);

    if (r->dst_resolved) {
        for (int i = 0; i < r->num; i++)
            free(r->dst_resolved[i]);
        free(r->dst_resolved);
    }

    if (r->error) free(r->error);
}






void end_backup(const char* src, const char* dest, struct backup_entry* backups, int* n){
    printf("end backup\n");



    for(int i = 0; i < *n; i++){
        if((strcmp(backups[i].dst_resolved, dest) == 0) && (strcmp(backups[i].src_resolved, src) == 0)){
            kill(backups[i].pid, SIGTERM);
                remove_backup(backups, n, backups[i].pid);
                //free(backups[i].src_resolved);
                //free(backups[i].dst_resolved);
            return;
        }
    }

fprintf(stderr,"end backup no dest\n");

}



void list_backups(struct backup_entry* backs, int* num){
    printf("list backups\n");

    if (*num == 0) {
        printf("No active backups\n");
        return;
    }

    for(int i = 0; i < *num; i++){
        printf(
            "%d %s -> %s\n",
            backs[i].pid,
            backs[i].src_resolved,
            backs[i].dst_resolved
        );
    }
    
}



void print_commands(){


    
}






char *join_path(const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b);
    char *res = malloc(la + lb + 2);
    if (!res) {
        perror("malloc");
        return NULL;
    }

    strcpy(res, a);
    if (la > 0 && a[la-1] != '/')
        strcat(res, "/");
    strcat(res, b);
    return res;
}





#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int cp(const char *to, const char *from)
{
    int fd_to, fd_from;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if (fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd_to < 0)
        goto out_error;

    while (nread = read(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (close(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);


        return 0;
    }

  out_error:
    saved_errno = errno;

    close(fd_from);
    if (fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}


#include <ftw.h>
#include <unistd.h>

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

int rmrf(char *path)
{
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}



void inotify_loop(const char *src_root, const char *dst_root){
    int notify_fd = inotify_init();
    if (notify_fd < 0) {
        perror("inotify_init");
        exit(EXIT_FAILURE);
    }


    struct WatchMap map = {0};

    add_watch_recursive(notify_fd, &map, src_root);

    char buf[EVENT_BUF_LEN];

    while (!terminate) {
        ssize_t len = read(notify_fd, buf, sizeof(buf));
        if (len < 0) {
            if (errno == EINTR) continue;

            perror("inotify read");
            break;
        }

        for (char *p = buf; p < buf + len; ) {
            struct inotify_event *ev = (struct inotify_event *)p;


            struct Watch *w = find_watch(&map, ev->wd);
            if (!w) return;

            char src_path[PATH_MAX];
            if (ev->len > 0)
                snprintf(src_path, sizeof(src_path), "%s/%s", w->path, ev->name);
            else
                snprintf(src_path, sizeof(src_path), "%s", w->path);


            const char *rel = src_path + strlen(src_root);
            if (*rel == '/') rel++;
            char *dst_path = join_path(dst_root, rel);

            if (ev->mask & (IN_CREATE | IN_MODIFY)) {
                struct stat st;
               if (lstat(src_path, &st) == 0) {
                if (S_ISREG(st.st_mode)) {
                    unlink(dst_path);
                    cp(dst_path, src_path);
                }
                else if (S_ISLNK(st.st_mode)) {
                    char buff2[PATH_MAX];
                    ssize_t len2 = readlink(src_path, buff2, sizeof(buff2)-1);
                    if (len2 >= 0) {
                        buff2[len2] = '\0';
                        unlink(dst_path);
                        symlink(buff2, dst_path);
                    }
                }
            }

            }
        
        
            if ((ev->mask & IN_CREATE) && (ev->mask & IN_ISDIR)) {
                mkdir(dst_path, 0777);
                add_watch_recursive(notify_fd, &map, src_path);
            }


            if (ev->mask & IN_MOVED_FROM) {
                pending_cookie = ev->cookie;
                strncpy(pending_old_path, src_path, sizeof(pending_old_path) - 1);
            }

            else if (ev->mask & IN_MOVED_TO) {
                if (pending_cookie == ev->cookie && pending_cookie != 0) {
                
                    update_watch_paths(&map, pending_old_path, src_path);
                
                    char *old_rel = pending_old_path + strlen(src_root);
                    if (*old_rel == '/') old_rel++;
                
                    char *new_rel = src_path + strlen(src_root);
                    if (*new_rel == '/') new_rel++;
                
                    char *old_dst = join_path(dst_root, old_rel);
                    char *new_dst = join_path(dst_root, new_rel);
                
                    rename(old_dst, new_dst);
                
                    free(old_dst);
                    free(new_dst);
                
                    pending_cookie = 0;
                    pending_old_path[0] = '\0';
                }
                else {
                    struct stat st;
                    if (lstat(src_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                        mkdir(dst_path, 0777);
                        add_watch_recursive(notify_fd, &map, src_path);
                    }
                }
            }
            if (ev->mask & IN_DELETE) {
            struct stat st;
            if (lstat(dst_path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    rmrf(dst_path);
                } else {
                    unlink(dst_path);
                }
            }
            if (ev->mask & IN_DELETE_SELF) {
                remove_from_map(&map, ev->wd);
            }

}       
            free(dst_path);

            p += sizeof(struct inotify_event) + ev->len;
        }
    }
    free_watch_map(&map);
    close(notify_fd);
}





int copy_tree(const char *src, const char *dst)
{
    if (terminate) return -1;

    struct stat st;

    if (lstat(src, &st) < 0) {
        perror(src);
        return -1;
    }

    if (S_ISDIR(st.st_mode)) {
        if (mkdir(dst, st.st_mode & 0777) < 0 && errno != EEXIST) {
            perror(dst);
            return -1;
        }

        DIR *dir = opendir(src);
        if (!dir) {
            perror(src);
            return -1;
        }

        struct dirent *ent;
        while ((ent = readdir(dir))) {
            if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
                continue;

            char *sp = join_path(src, ent->d_name);
            char *dp = join_path(dst, ent->d_name);

            // if(sp == -1){
            //     perror(sp);
            //     return -1;
            // }
            // if(dp == -1){
            //     perror(dp);
            //     return -1;
            // }

            if (copy_tree(sp, dp) < 0) {
                free(sp);
                free(dp);
                closedir(dir);
                return -1;
            }

            free(sp);
            free(dp);
        }
        closedir(dir);
    }
    else if (S_ISREG(st.st_mode)) {

        if (cp(dst, src) < 0) {
            perror(dst);
            return -1;
        }
    }
    else if (S_ISLNK(st.st_mode)) {
        char buf[PATH_MAX];
        ssize_t len = readlink(src, buf, sizeof(buf)-1);
        if (len < 0) {
            perror(src);
            return -1;
        }
        buf[len] = '\0';

        if (symlink(buf, dst) < 0) {
            perror(dst);
            return -1;
        }
    }

    return 0;
}


int dir_is_empty(const char *path)
{
    DIR *d = opendir(path);
    if (!d) return -1;

    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcmp(e->d_name, ".") &&
            strcmp(e->d_name, "..")) {
            closedir(d);
            return 0;
        }
    }

    closedir(d);
    return 1;
}




int add_backup(struct backup_entry *r)
{
    sethandler(term_handler, SIGTERM);
    sethandler(term_handler, SIGINT);

    struct stat st;
    if (stat(r->dst_resolved, &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            fprintf(stderr, "dst exists and is not a directory\n");
            return -1;
        }
        if (!dir_is_empty(r->dst_resolved)) {
            fprintf(stderr, "dst directory not empty\n");
            return -1;
        }
    }
    sleep(4);


    if (copy_tree(r->src_resolved, r->dst_resolved) < 0) {
        fprintf(stderr, "backup failed\n");
        return -1;
    }

    inotify_loop(r->src_resolved, r->dst_resolved);

    return 0;
}



void kill_children(struct backup_entry *backs, int *n) {
    for (int i = 0; i < *n; ++i) {
        if (backs[i].pid > 0) {
            kill(backs[i].pid, SIGTERM);
        }
    }

    int status;
    pid_t w;
    while ((w = waitpid(-1, &status, 0)) > 0) {
        remove_backup(backs, n, w);
    }

    for (int i = 0; i < *n; ++i) {
        free(backs[i].src_resolved);
        free(backs[i].dst_resolved);
    }
    *n = 0;
}






int restore(const char *dst, const char *src)
{
    struct stat ssb, dsb;

    if (lstat(src, &ssb) < 0) {
        perror(src);
        return -1;
    }

    if (lstat(dst, &dsb) < 0) {
        if (errno == ENOENT) {
            if (S_ISDIR(ssb.st_mode)) {
                if (mkdir(dst, ssb.st_mode & 0777) < 0) {
                    perror(dst);
                    return -1;
                }
                return restore(dst, src);
            } 
            else if (S_ISREG(ssb.st_mode)) {
                return cp(dst, src);
            }
            else if (S_ISLNK(ssb.st_mode)) {
                char buf[PATH_MAX];
                ssize_t len = readlink(src, buf, sizeof(buf)-1);
                if (len < 0) return -1;
                buf[len] = '\0';
                return symlink(buf, dst);
            }
            return 0;
        }
        perror(dst);
        return -1;
    }

    if ((ssb.st_mode & S_IFMT) != (dsb.st_mode & S_IFMT)) {
        rmrf((char*)dst);
        return restore(dst, src);
    }

    if (S_ISDIR(ssb.st_mode)) {
        DIR *d = opendir(src);
        if (!d) return -1;

        struct dirent *e;

        while ((e = readdir(d))) {
            if (!strcmp(e->d_name,".") || !strcmp(e->d_name,".."))
                continue;

            char *sp = join_path(src, e->d_name);
            char *dp = join_path(dst, e->d_name);

            if (restore(dp, sp) < 0) {
                free(sp); free(dp);
                closedir(d);
                return -1;
            }

            free(sp); free(dp);
        }
        closedir(d);

        DIR *dd = opendir(dst);
        if (!dd) return -1;

        while ((e = readdir(dd))) {
            if (!strcmp(e->d_name,".") || !strcmp(e->d_name,".."))
                continue;

            char *dp = join_path(dst, e->d_name);
            char *sp = join_path(src, e->d_name);

            if (lstat(sp, &ssb) < 0 && errno == ENOENT) {
                rmrf(dp);
            }

            free(sp); free(dp);
        }
        closedir(dd);
        return 0;
    }

    if (S_ISREG(ssb.st_mode)) {
        if ((ssb.st_size != dsb.st_size)) {
            unlink(dst);
            return cp(dst, src);
        }
        return 0;
    }

    if (S_ISLNK(ssb.st_mode)) {
        char a[PATH_MAX], b[PATH_MAX];
        ssize_t la = readlink(src, a, sizeof(a)-1);
        ssize_t lb = readlink(dst, b, sizeof(b)-1);
        if (la < 0) return -1;
        a[la] = '\0';

        if (lb < 0 || strcmp(a, b) != 0) {
            unlink(dst);
            return symlink(a, dst);
        }
        return 0;
    }

    return 0;
}


void restore_backup(struct parse_result *r){
    printf("restore backup\n");

        if (restore(r->src_resolved, r->dst_resolved[0]) < 0) {
        fprintf(stderr, "restore failed\n");
    }
}






int main(void)
{
    char line[1024];
    struct parse_result r;
    int backup_count = 0;
    struct backup_entry backups[128];
    //int buff2[1024];

    //sethandler(SIG_IGN, SIGINT);
    //sethandler(SIG_IGN, SIGTERM);

    sethandler(term_handler, SIGINT);
    sethandler(term_handler, SIGTERM);


    while (!terminate) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;

        if (parse_input(line, backups, backup_count, &r) < 0) {
            fprintf(stderr, "error: %s\n", r.error);
            free_parse_result(&r);
            continue;
        }


        if(r.cmd == CMD_ADD){
            for(int i = 0; i < r.num; i++){
            
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                free_parse_result(&r);
                continue;
            }
            else if (pid == 0){
                struct backup_entry bak;
                bak.src_resolved = strdup(r.src_resolved);
                bak.dst_resolved = strdup(r.dst_resolved[i]);
                bak.pid = getpid();

                printf("%d %s %s \n", getpid(), bak.src_resolved, bak.dst_resolved);
                
                int rc = add_backup(&bak);
                if (rc < 0) {
                    fprintf(stderr, "child: copy failed\n");
                    exit(EXIT_FAILURE);
                }
                //printf("%d finidhed backup\n", pid);
                exit(EXIT_SUCCESS);
            }
            else {
                backups[backup_count].src_resolved = strdup(r.src_resolved);
                backups[backup_count].dst_resolved = strdup(r.dst_resolved[i]);
                backups[backup_count].pid = pid;
                backup_count++;
                                printf("%d started backup\n", pid);
                

            }       
        }
            
        }
        else if(r.cmd == CMD_END){
            end_backup(r.src_resolved, r.dst_resolved[0], backups, &backup_count);
        }
        else if(r.cmd == CMD_LIST){
            list_backups(backups, &backup_count);
        }
        else if(r.cmd == CMD_RESTORE){
            restore_backup(&r);
        }
        else if(r.cmd == CMD_EXIT){
            kill_children(backups, &backup_count);
            free_parse_result(&r);
            return 0;
        }
        else if(r.cmd == CMD_INVALID){
        }
        


        printf("backup count: %d\n", backup_count);
        reap_children(backups, &backup_count);
        free_parse_result(&r);
    }
    kill_children(backups, &backup_count);

    return EXIT_SUCCESS;
}
