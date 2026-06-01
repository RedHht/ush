#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_LENGTH 1024
#define ARG_MAX 32 // Cantidad máxima de argumentos.

void mostrar_shell();

int get_args(char *command, char *args[]);

int comando_incorporado(char *args[], int argc);

int comando_externo(char *args[], int argc);

void inode(char *args[], int argc);

void procmem(char *args[], int argc);

char hostname[32];
struct passwd *passwd; // Usuario que esta corriendo la shell.

int main(void) {
    char command[MAX_LENGTH];
    char *argv[ARG_MAX];

    int running = 1;

    gethostname(hostname, 32);
    passwd = getpwuid(getuid());
    pid_t PID_padre = getpid();

    printf("El PID del proceso padre es: %d \n", PID_padre);
    printf("μsh - UADE Shell \n");
    printf("Comandos incorporados: cd, pwd, procmem <pid>, inode <path>, exit\n");

    while (running) {
        mostrar_shell();

        int argc = 0;
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] =
                '\0'; // Elimina el salto de línea al final del input.
        // strcspn devuelve la longitud de la cadena hasta un \n, se reemplaza por
        // el caracter nulo \0

        if (strcmp(command, "") == 0) // Si no se ingresa nada
        {
            continue;
        }

        argc = get_args(
            command,
            argv); // Establece los argumentos en argv y devuelve la cantidad.

        int codigo_salida = comando_incorporado(argv, argc);

        if (codigo_salida == -1) {
            running = 0;
        } else if (codigo_salida == 0) {
            if (comando_externo(argv, argc)) {
                running = 0;
            }
        }
    }

    return 0;
}

int get_args(char *command, char *args[]) {
    char *token = strtok(command, " \t");

    int i = 0;
    for (i = 0; token != NULL; i++) {
        args[i] = token;
        token = strtok(NULL, " \t");
    }

    args[i] = NULL; // NECESARIO PARA QUE EXECVP SEPA CUANDO TERMINA LA LISTA DE
    // ARGUMENTOS!
    return i;
}

void mostrar_shell() {
    char wd[MAX_LENGTH];

    if (getcwd(wd, MAX_LENGTH) != NULL) {
        printf("(μsh) [%s@%s %s]$ ", passwd->pw_name, hostname, wd);
    } else {
        printf("(μsh) [%s@%s ]$ ", passwd->pw_name, hostname);
    }
}

int comando_incorporado(char *args[],
                        int argc) // Si el comando es incorpotado devuelve 1, si
// hay que terminar el programa -1, si no 0.
{
    if (strcmp(args[0], "exit") == 0) {
        return -1;
    }

    if (strcmp(args[0], "cd") == 0) {
        if (argc == 1) {
            chdir(passwd->pw_dir);
            return 1;
        }

        if (chdir(args[1]) == 0) {
            return 1;
        }

        printf("cd: No existe el archivo o directorio: %s\n", args[1]);
        return 1;
    }

    if (strcmp(args[0], "pwd") == 0) {
        char working_dir[MAX_LENGTH];

        printf("%s \n", getcwd(working_dir, MAX_LENGTH));
        return 1;
    }

    if (strcmp(args[0], "procmem") == 0) {
        procmem(args, argc);
        return 1;
    }

    if (strcmp(args[0], "inode") == 0) {
        inode(args, argc);
        return 1;
    }

    return 0; // El comando no es incorporado
}

void inode(char *args[], int argc) {
    if (argc == 1) {
        printf("Ingrese un archivo. Uso: inode <pid> \n");
        return;
    }

    struct stat buffer;
    if (stat(args[1], &buffer) == -1) {
        printf("El archivo ingresado es invalido. \n");
        return;
    }

    printf("################################################ \n"
        "Analizando archivo \"%s\" \n\n"
        "i-node: %lu \n"
        "Dispositivo: %lu \n"
        "ID del usuario dueño: %d \n"
        "ID del grupo dueño: %d \n"
        "Espacio total, en bytes: %ld \n\n"
        "################################################ \n"
        , args[1], buffer.st_ino, buffer.st_dev, buffer.st_uid, buffer.st_gid, buffer.st_size);
}

void procmem(char *args[], int argc) {
    if (argc == 1) {
        printf("Ingrese un PID. Uso: procmem <pid> \n");
        return;
    }

    int pid = atoi(args[1]);

    if (pid <= 0)
    {
        printf("Ingrese un número de PID válido. Uso: procmem <pid> \n");
        return;
    }

    char path[MAX_LENGTH];
    sprintf(path, "/proc/%s/maps", args[1]);
    
    FILE* file = fopen(path, "r");
    char buffer[500];

    if (file == NULL) {
        printf("No existe un proceso con ese PID. \n");
        return;
    }

    printf("Mostrando mapeos de memoria del proceso: \n\n");

    printf("Memoria Virtual                         Permisos  Offset    Device inode  Tipo  Archivo \n");

    while (fgets(buffer, 500, file)) {
        char memory[64] = "";
        char permissions[64] = "";
        char offset[64] = "";
        char device[64] = "";
        char inode[64] = "";
        char charged_file[64] = "";
        sscanf(buffer, "%s %s %s %s %s %s", memory, permissions, offset, device, inode, charged_file);

        char *tipo = "";
        if      (strcmp(charged_file, "[heap]") == 0)  tipo = "HEAP";
        else if (strcmp(charged_file, "[stack]") == 0) tipo = "STACK";
        else if (strcmp(charged_file, "[vdso]") == 0)  tipo = "VDSO";
        else if (strcmp(charged_file, "[vvar]") == 0)  tipo = "VVAR";
        else if (strstr(charged_file, ".so") != NULL)  tipo = "LIB";
        else if (strlen(charged_file) > 0)             tipo = "BIN";
        else                                           tipo = "ANON";

        printf("%-39s %-9s %-9s %-6s %-6s %-5s %s\n", memory, permissions, offset, device, inode, tipo, charged_file);
    }

    fclose(file);

    printf("\nMostrando consumo de memoria total: \n\n");

    sprintf(path, "/proc/%s/status", args[1]);
    
    FILE* status = fopen(path, "r");

    char line[256];
    while (fgets(line, sizeof(line), status)) {
        if (strncmp(line, "VmSize", 6) == 0)
            printf("  Espacio virtual total:       %s", line + 7); // +7 saltea "VmSize:"
        else if (strncmp(line, "VmRSS", 5) == 0)
            printf("  En RAM fisica ahora mismo:   %s", line + 6); // +6 saltea "VmRSS:"
        else if (strncmp(line, "VmSwap", 6) == 0)
            printf("  Enviado a swap:              %s", line + 7); // +7 saltea "VmSwap:"
    }

    fclose(status);
}

int comando_externo(char *args[], int argc) {
    pid_t pid = fork();

    if (pid < 0) {
        printf("No fue posible generar un proceso nuevo.");
        return 1;
    }

    if (pid == 0) // EJECUCIÓN DEL PROCESO EXTERNO (hijo)
    {
        execvp(args[0],
               args); // A diferencia de execve, execvp busca en el path del sistema
        // el binario y no requiere la ruta completa.

        // Si llegamos hasta aca, no se pudo correr la imagen en el proceso.
        printf("μsh: comando no encontrado: %s \n", args[0]);

        return 1;
    } else // EJECUCIÓN DEL PROCESO DE SHELL (padre)
    {
        int retorno;

        waitpid(pid, &retorno, 0); // Espera a que termine el proceso hijo
    }

    return 0;
}
