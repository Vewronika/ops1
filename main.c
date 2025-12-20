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
//#include "parser.h"


#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), kill(0, SIGKILL), exit(EXIT_FAILURE))


#ifndef PATH_MAX
# define PATH_MAX 4096
#endif

enum cmd_type { CMD_INVALID=0, CMD_ADD, CMD_END, CMD_LIST, CMD_RESTORE, CMD_EXIT };

struct backup_entry { char *src_resolved; char *dst_resolved; };

struct parse_result {
    enum cmd_type cmd;
    char *src, *dst; 
    char *src_resolved, *dst_resolved;
    int src_is_symlink;
    int id;
    char *error;
};

static void set_error(struct parse_result *r, const char *msg)
{
    free(r->error);
    r->error = strdup(msg);
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




static int canonicalize_allow_nonexistent(const char *in, char *out, size_t olen) {
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
        r->error = terr; return -1;
    }

    if (tc==0) { 
        printf("empty\n");
        goto out; 
    }

    char cmd[32]; snprintf(cmd, sizeof(cmd), "%s", toks[0]);
    for (char *c=cmd;*c;c++) if (*c>='A'&&*c<='Z') *c += 'a'-'A';

    if (!strcmp(cmd,"add")) {
        if (tc!=3) { r->error=strdup("usage: add <src> <dst>"); goto out; }
        r->src=strdup(toks[1]); r->dst=strdup(toks[2]);

        struct stat st;
        if (lstat(r->src,&st)!=0) { set_error(r,"src invalid"); goto out; }
        r->src_is_symlink = S_ISLNK(st.st_mode);

        char buf[PATH_MAX];
        if (!realpath(r->src, buf)) { set_error(r,"src resolve"); goto out; }

        r->src_resolved=strdup(buf);

        if (canonicalize_allow_nonexistent(r->dst, buf, sizeof(buf))!=0) { 
                set_error(r,"dst invalid");
                goto out; 
            }
        r->dst_resolved=strdup(buf);

        if (!strcmp(r->src_resolved, r->dst_resolved) || path_is_prefix(r->src_resolved, r->dst_resolved) || path_is_prefix(r->dst_resolved, r->src_resolved) ||
            (existing && is_duplicate_pair(existing, existing_n, r->src_resolved, r->dst_resolved))) {

            set_error(r,"invalid src/dst pair");
            goto out;
        }

        r->cmd = CMD_ADD;

    } 
    else if (!strcmp(cmd,"end")) {
        if (tc!=2) { r->error=strdup("usage: end <id>"); goto out; }
        r->id = atoi(toks[1]); r->cmd = CMD_END;

    } else if (!strcmp(cmd,"list")) {
        if (tc!=1) { r->error=strdup("usage: list"); goto out; }
        r->cmd = CMD_LIST;

    } else if (!strcmp(cmd,"restore")) {
        if (tc!=2 && tc!=3) { r->error=strdup("usage: restore <id> [dst]"); goto out; }
        r->id = atoi(toks[1]);
        if (tc==3) {
            r->dst=strdup(toks[2]);
            char buf[PATH_MAX];
            if (canonicalize_allow_nonexistent(r->dst, buf, sizeof(buf))!=0)
                { set_error(r,"dst invalid"); goto out; }
            r->dst_resolved=strdup(buf);
        }
        r->cmd = CMD_RESTORE;

    } else if (!strcmp(cmd,"exit")) {
        if (tc!=1) { r->error=strdup("usage: exit"); goto out; }
        r->cmd = CMD_EXIT;
    } else {
        set_error(r,"unknown command");
    }

out:
    for (int i=0;i<tc;i++) free(toks[i]);
    free(toks);
    return r->error ? -1 : 0;
}

void free_parse_result(struct parse_result *r) {
    free(r->src); free(r->dst);
    free(r->src_resolved); free(r->dst_resolved);
    free(r->error);
    memset(r,0,sizeof(*r));
}



void add_backup(struct parse_result *r){
    printf("src: %s\n dst: %s\n", r->src_resolved, r->dst_resolved);
    printf("add backup\n");
}

void end_backup(int id){
    printf("end backup\n");
}

void list_backups(){
    printf("list backups\n");
}

void restore_backup(struct parse_result *r){
    printf("restore backup\n");
}

void print_commands(){
    
}

char *join_path(const char *a, const char *b)
{
    size_t la = strlen(a), lb = strlen(b);
    char *res = malloc(la + lb + 2);
    if (!res) ERR("malloc");

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

        /* Success! */
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



void copy_dir(char* from, char* to){
    printf("copying \n");


    DIR *dirp;
    struct dirent *dp;
    struct stat filestat;
    int dirs = 0, files = 0, links = 0, other = 0;
    if ((dirp = opendir(r->src_resolved)) == NULL)
        ERR("opendir");
    do
    {
        errno = 0;
        if ((dp = readdir(dirp)) != NULL)
        {
            if (lstat(dp->d_name, &filestat))
                ERR("lstat");
            if (S_ISDIR(filestat.st_mode)){
                dirs++;
                copy_dir();
            }

            else if (S_ISREG(filestat.st_mode))
            {
                files++;

            }

            else if (S_ISLNK(filestat.st_mode))
                links++;
            else
                other++;
        }
    } while (dp != NULL);

    if (errno != 0)
        ERR("readdir");
    if (closedir(dirp))
        ERR("closedir");
    printf("Files: %d, Dirs: %d, Links: %d, Other: %d\n", files, dirs, links, other);


}






int main(void)
{
    char line[1024];
    struct parse_result r;
    int backup_count = 0;
    struct backup_entry backups[128];

    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;

        if (parse_input(line, backups, backup_count, &r) < 0) {
            fprintf(stderr, "error: %s\n", r.error);
            free_parse_result(&r);
            continue;
        }

        switch (r.cmd) {
            case CMD_ADD:
                add_backup(&r);
                break;
            case CMD_END:
                end_backup(r.id);
                break;
            case CMD_LIST:
                list_backups();
                break;
            case CMD_RESTORE:
                restore_backup(&r);
                break;
            case CMD_EXIT:
                free_parse_result(&r);
                return 0;
            case CMD_INVALID:
                break;
        }

        free_parse_result(&r);
    }

    return EXIT_SUCCESS;
}
