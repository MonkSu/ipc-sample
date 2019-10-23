#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <memory.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/sem.h>　
#include <sys/msg.h>　


using namespace std;

#define IPC_FIFO_PATH                   "/tmp/ipc.fifo"
#define IPC_DATA_LEN                    11

#define IPC_ROLE_WRITE                  1
#define IPC_ROLE_READ                   2


#define IPC_SHM_KEY_PATH                "/proc/cpuinfo"
#define IPC_SHM_SIZE                    (1024*4)


#define IPC_SEM_KEY_PATH                "/proc/cpuinfo"


struct ipc_sysv_msgbuf {
    long mtype;
    char mdata[IPC_DATA_LEN];
};


static string type = "fifo";
static string protocol = "sysv";
static int fifo_fd = -1;
static char *p_shm = NULL;
static int sem_id = -1;
static int msg_id = -1;
static int shm_id = -1;
static char data[IPC_DATA_LEN] = "0123456789";


static int shm_posix_id = -1;
static char *p_shm_posix = NULL;

static void usage(string command)
{
    printf(\
"\nUsage: %s [OPTION]...\n"
"-h,        help\n"
"-w,        write process\n"
"-r,        read  process\n"
"-t         IPC type: fifo/shm/sem/msg \n"
"-p         protocol: sysv/posix\n"
"such as:   ipc_sysv_sample -w -t fifo -p sysv\n\n"
, command.c_str());

}

static void handler(int signo)
{
    if (fifo_fd != -1) {
        close(fifo_fd);
    }

    if (p_shm) {
        shmdt(p_shm);
    }

    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID, 0);
    }

    if (shm_id != -1) {
        semctl(shm_id, 0, IPC_RMID, 0);
    }

    if (msg_id != -1) {
        semctl(msg_id, IPC_RMID, 0);
    }

    exit(1);
}

static void signal_handler(int signo)
{
    printf("signal: %d\n", signo);
}

__attribute__((unused)) static int ipc_sysv_fifo_write(void)
{
    int ret;

    if (access(IPC_FIFO_PATH, F_OK) == 0) {
        unlink(IPC_FIFO_PATH);
    }

    ret = mkfifo(IPC_FIFO_PATH, 0666);
    if (ret) {
        printf("Failed to mkfifo. %s\n", strerror(errno));
        return -1;
    }

    fifo_fd = open(IPC_FIFO_PATH, O_WRONLY);
    if (fifo_fd < 0) {
        printf("Failed to open %s. %s\n", IPC_FIFO_PATH, strerror(errno));
        return -1;
    }

    while(1) {
        ret = write(fifo_fd, data, sizeof(data));
        if (ret < 0) {
            printf("Failed to write data %s\n", strerror(errno));
        } else {
            printf("Write %s\n", data);
        }

        sleep(3);
    }

    return 0;
}

__attribute__((unused)) static int ipc_sysv_fifo_read(void)
{
    int ret;
    char buf[IPC_DATA_LEN] = {0};

    fifo_fd = open(IPC_FIFO_PATH, O_RDONLY);
    if (fifo_fd < 0) {
        printf("Failed to open %s. %s\n", IPC_FIFO_PATH, strerror(errno));
        return -1;
    }

    while(1) {
        memset(buf, 0, sizeof(buf));
        ret = read(fifo_fd, buf, sizeof(buf));
        if (ret <= 0) {
            printf("Failed to read data. %s\n", strerror(errno));
        } else {
            printf("Read %s\n", buf);
        }

        sleep(3);
    }
    return 0;
}

__attribute__((unused)) static int ipc_sysv_shm_write(void)
{
    key_t key = ftok(IPC_SHM_KEY_PATH, 0x110);
    int count = 0;

    if (key < 0) {
        printf("Failed to ftok. %s\n", strerror(errno));
        return -1;
    }
    printf("shm key %ld. \n", key);
    shm_id = shmget(key, IPC_SHM_SIZE, IPC_CREAT|IPC_EXCL|0666);
    if (shm_id < 0) {
        if (errno == EEXIST) {
            printf("shm is exited. %s\n", strerror(errno));
            shm_id = shmget(key, 0, 0666);
            if (shm_id < 0) {
                printf("Failed to shmget. %s\n", strerror(errno));
                return -1;
            }
        }
    }
    p_shm = (char *)shmat(shm_id, NULL, 0);
    if (p_shm == NULL) {
        printf("Failed to shmat. %s\n", strerror(errno));
        return -1;
    }

    while(1) {
        count++;
        memcpy(p_shm, data, sizeof(data)>IPC_SHM_SIZE?IPC_SHM_SIZE:sizeof(data));
        data[0] = (count&0x07) + '0';
        printf("copy data: %s\n", data);
        sleep(3);
    }
    return 0;
}

__attribute__((unused)) static int ipc_sysv_shm_read(void)
{
    char buf[IPC_DATA_LEN] = {0};

    key_t key = ftok(IPC_SHM_KEY_PATH, 0x110);
    if (key < 0) {
        printf("Failed to ftok. %s\n", strerror(errno));
        return -1;
    }
    printf("shm key %ld. \n", key);
    shm_id = shmget(key, 0, 0666);
    if (shm_id < 0) {
        printf("Failed to shmget. %s\n", strerror(errno));
        return -1;
    }
    p_shm = (char *)shmat(shm_id, NULL, 0);
    if (p_shm == NULL) {
        printf("Failed to shmat. %s\n", strerror(errno));
        return -1;
    }

    while(1) {
        if(strlen(p_shm) > 0 && strcmp(p_shm, buf)) {
            memcpy(buf, p_shm, sizeof(buf));
            printf("read data: %s\n", buf);
        } else {
            printf("shm is empty. \n");
        }

        sleep(3);
    }
    return 0;
}


__attribute__((unused)) static int ipc_sysv_msg_write(void)
{
    int ret;
    int count = 0;
    struct ipc_sysv_msgbuf msg;
    key_t key = ftok(IPC_SEM_KEY_PATH, 0x111);
    if (key < 0) {
        printf("Failed to ftok. %s\n", strerror(errno));
        return -1;
    }

    msg_id = msgget(key, IPC_CREAT|IPC_EXCL|0666);
    if (msg_id < 0) {
        if (errno == EEXIST) {
            printf("msg is exited. %s\n", strerror(errno));
            msg_id = msgget(key, 0666);
            if (msg_id < 0) {
                printf("Failed to msgget. %s\n", strerror(errno));
                return -1;
            }
        }
    }

    msg.mtype = 1;
    memcpy(msg.mdata, data, sizeof(msg.mdata));
    while(1) {
        count++;
        msg.mdata[0] = (count&0x07) + '0';
        ret = msgsnd(msg_id, (void*)&msg, sizeof(msg), 0);
        if (ret < 0) {
            printf("Failed to msgsnd. %s\n", strerror(errno));
            sleep(3);
            continue;
        }
        printf("msg send data: %s\n", msg.mdata);
        sleep(3);
    }
}

__attribute__((unused)) static int ipc_sysv_msg_read(void)
{
    int ret;
    char buf[IPC_DATA_LEN] = {0};
    struct ipc_sysv_msgbuf msg = {0};
    struct msqid_ds msg_ds = {0};
    key_t key = ftok(IPC_SEM_KEY_PATH, 0x111);
    if (key < 0) {
        printf("Failed to ftok. %s\n", strerror(errno));
        return -1;
    }

    msg_id = msgget(key, 0666);
    if (msg_id < 0) {
        printf("Failed to msgget. %s\n", strerror(errno));
        return -1;
    }

    while(1) {
        memset(msg.mdata, 0, sizeof(msg.mdata));
        ret = msgrcv(msg_id, (void*)&msg, sizeof(msg), 0, 0);
        if (ret < 0) {
            printf("Failed to msgrcv. %s\n", strerror(errno));
            sleep(1);
            continue;
        }
        printf("msg recv data: %s\n", msg.mdata);
        ret = msgctl(msg_id, IPC_STAT, &msg_ds);
        if (ret < 0) {
            printf("Failed to msgctl. %s\n", strerror(errno));
        } else {
            printf("UID: msg_qbytes: %d\n", msg_ds.msg_qbytes);
            printf("UID: msg_lspid : %d\n", msg_ds.msg_lspid);
            printf("UID: msg_lrpid : %d\n", msg_ds.msg_lrpid);
        }

        sleep(1);
    }
}


__attribute__((unused)) static int ipc_sysv_sem_write(void)
{
    int ret;
    struct sembuf sem_buf;

    key_t key = ftok(IPC_SEM_KEY_PATH, 0x111);
    if (key < 0) {
        printf("Failed to ftok. %s\n", strerror(errno));
        return -1;
    }
    printf("sem key %ld. \n", key);
    sem_id = semget(key, 1, IPC_CREAT|IPC_EXCL|0666);
    if (sem_id < 0) {
        if (errno == EEXIST) {
            printf("shm is exited. %s\n", strerror(errno));
            sem_id = semget(key, 1, 0666);
            if (sem_id < 0) {
                printf("Failed to semget. %s\n", strerror(errno));
                return -1;
            }
        }
    }
    semctl(sem_id, 0, SETVAL, 1);
    while(1) {
#define SEM_P      (-1)
#define SEM_V      (1)
#define SEM_PV(pv) {                            \
            sem_buf.sem_num = 0;                \
            sem_buf.sem_op  = pv;               \
            sem_buf.sem_flg = SEM_UNDO;         \
            ret = semop(sem_id, &sem_buf, 1);   \
        }

        printf("To get sem\n");
        SEM_PV(SEM_P);
        if (ret < 0) {
            printf("Failed to semop P. %s\n", strerror(ret));
            sleep(3);
            continue;
        }
        printf("Get sem 3s\n");
        sleep(3);
        printf("release sem 1s\n");
        SEM_PV(SEM_V);
        if (ret < 0) {
            printf("Failed to semop. %s\n", strerror(ret));
            sleep(1);
            continue;
        }
        sleep(1);
    }
    return 0;
}

__attribute__((unused)) static int ipc_sysv_sem_read(void)
{
    int ret;
    struct sembuf sem_buf;

    key_t key = ftok(IPC_SEM_KEY_PATH, 0x111);
    if (key < 0) {
        printf("Failed to ftok. %s\n", strerror(errno));
        return -1;
    }
    printf("sem key %ld. \n", key);
    sem_id = semget(key, 1, IPC_CREAT|IPC_EXCL|0666);
    sem_id = semget(key, 1, 0666);
    if (sem_id < 0) {
        printf("Failed to semget. %s\n", strerror(errno));
        return -1;
    }

    while(1) {
#define SEM_P      (-1)
#define SEM_V      (1)
#define SEM_PV(pv) {                            \
            sem_buf.sem_num = 0;                \
            sem_buf.sem_op  = pv;               \
            sem_buf.sem_flg = SEM_UNDO;         \
            ret = semop(sem_id, &sem_buf, 1);   \
        }

        printf("To get sem\n");
        SEM_PV(SEM_P);
        if (ret < 0) {
            printf("Failed to semop P. %s\n", strerror(ret));
            continue;
        }
        printf("Get sem 1s\n");
        sleep(1);
        printf("release sem\n");
        SEM_PV(SEM_V);
        if (ret < 0) {
            printf("Failed to semop. %s\n", strerror(ret));
            continue;
        }
    }
    return 0;
}

__attribute__((unused)) static int ipc_posix_shm_write(void)
{
    int ret = 0;
    int count = 0;
    struct stat buf;

    shm_posix_id = shm_open("shm.posix", O_CREAT|O_RDWR, 0777);
    if (shm_posix_id < 0) {
        printf("Failed to shm_open. %s\n", strerror(errno));
        return -1;
    }

    ret = ftruncate(shm_posix_id, IPC_SHM_SIZE);
    if (ret < 0) {
        printf("Failed to ftruncate. %s\n", strerror(errno));
        return -1;
    }

    ret = fstat(shm_posix_id, &buf);
    if (ret < 0) {
        printf("Failed to fstat. %s\n", strerror(errno));
        return -1;
    }
    printf("File size: %ld mode: %o\n",buf.st_size,buf.st_mode & 0777);
    p_shm_posix = (char*)mmap(NULL, buf.st_size, PROT_WRITE|PROT_READ, MAP_SHARED, shm_posix_id, 0);
    if (p_shm_posix == MAP_FAILED) {
        printf("Faile to mmap. %s\n", strerror(errno));
        return -1;
    }

    while(1) {
        count++;
        memcpy(p_shm_posix, data, sizeof(data)>IPC_SHM_SIZE?IPC_SHM_SIZE:sizeof(data));
        data[0] = (count&0x07) + '0';
        printf("copy data: %s\n", data);
        sleep(3);
    }
    return 0;
}

__attribute__((unused)) static int ipc_posix_shm_read(void)
{
    int ret = 0;
    int count = 0;
    struct stat buf;
    char dbuf[IPC_DATA_LEN] = {0};

    shm_posix_id = shm_open("shm.posix",O_CREAT | O_RDWR, 0777);
    if (shm_posix_id < 0) {
        printf("Failed to shm_open. %s\n", strerror(errno));
        return -1;
    }

    ret = fstat(shm_posix_id, &buf);
    if (ret < 0) {
        printf("Failed to fstat. %s\n", strerror(errno));
        return -1;
    }
    printf("File size: %ld mode: %o\n",buf.st_size,buf.st_mode & 0777);
    p_shm_posix = (char*)mmap(NULL, buf.st_size, PROT_WRITE, MAP_SHARED, shm_posix_id, 0);
    if (p_shm_posix == MAP_FAILED) {
        printf("Faile to mmap. %s\n", strerror(errno));
        return -1;
    }

    while(1) {
        if(strlen(p_shm_posix) > 0 && strcmp(p_shm_posix, dbuf)) {
            memcpy(dbuf, p_shm_posix, sizeof(dbuf));
            printf("read data: %s\n", dbuf);
        } else {
            printf("posix shm is empty. \n");
        }

        sleep(3);
    }
    return 0;
}



#define IPC_WRITE(protocol,type)         ipc_##protocol##_##type##_write()
#define IPC_READ(protocol,type)          ipc_##protocol##_##type##_read()

int main(int argc, char const *argv[])
{
    int ret = -1;
    int role = 0;

    string command = argv[0];

    if (argc < 3) {
        usage(command);
        return 0;
    }

    sigset(SIGINT, handler);
    sigset(SIGPIPE, signal_handler);

    while (1) {
        string options = "hwrp:t:";
        int oc = getopt(argc, (char *const *)argv, options.c_str());
        if(-1 == oc)
            break;

        switch (oc) {
        case 'h':
            usage(command);
            return 0;
        case 'w':
            role = IPC_ROLE_WRITE;
            break;
        case 'r':
            role = IPC_ROLE_READ;
            break;
        case 'p':
            protocol = optarg;
            break;
        case 't':
            type = optarg;
            break;
        default:
            usage(command);
            exit(0);
            break;
        }
    }
    switch(role) {
        case IPC_ROLE_WRITE:
            if (!strcmp(protocol.c_str(), (char*)"sysv")) {
                if (!strcmp(type.c_str(), (char*)"fifo")) {
                    IPC_WRITE(sysv, fifo);
                } else if (!strcmp(type.c_str(), (char*)"shm")) {
                    IPC_WRITE(sysv, shm);
                } else if (!strcmp(type.c_str(), (char*)"sem")) {
                    IPC_WRITE(sysv, sem);
                } else if (!strcmp(type.c_str(), (char*)"msg")) {
                    IPC_WRITE(sysv, msg);
                }
            } else {
                if (!strcmp(type.c_str(), (char*)"fifo")) {
                    IPC_WRITE(posix, fifo);
                } else if (!strcmp(type.c_str(), (char*)"shm")) {
                    IPC_WRITE(posix, shm);
                } else if (!strcmp(type.c_str(), (char*)"sem")) {
                    IPC_WRITE(posix, sem);
                } else if (!strcmp(type.c_str(), (char*)"msg")) {
                    IPC_WRITE(posix, msg);
                }
            }


        break;

        case IPC_ROLE_READ:
            if (!strcmp(protocol.c_str(), (char*)"sysv")) {
                if (!strcmp(type.c_str(), (char*)"fifo")) {
                    IPC_READ(sysv, fifo);
                } else if (!strcmp(type.c_str(), (char*)"shm")) {
                    IPC_READ(sysv, shm);
                } else if (!strcmp(type.c_str(), (char*)"sem")) {
                    IPC_READ(sysv, sem);
                } else if (!strcmp(type.c_str(), (char*)"msg")) {
                    IPC_READ(sysv, msg);
                }
            } else {
                if (!strcmp(type.c_str(), (char*)"fifo")) {
                    // IPC_READ(posix, fifo);
                } else if (!strcmp(type.c_str(), (char*)"shm")) {
                    IPC_READ(posix, shm);
                } else if (!strcmp(type.c_str(), (char*)"sem")) {
                    // IPC_READ(posix, sem);
                } else if (!strcmp(type.c_str(), (char*)"msg")) {
                    // IPC_READ(posix, msg);
                }
            }
        break;

        default:
            usage(command);
            return 0;
        break;
    }

    return 0;
}