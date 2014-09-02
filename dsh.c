/**
 * Copyright (C) 2014  Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <readline/readline.h>
#include <readline/history.h>

/** Definiciones */
#define SHELL "dsh"

/** Estructura que representa un comando que se ejecutará */
struct cmd {
	char *name;
	char *path;
	unsigned short int argc;
	char **argv;
	unsigned short int rc;
};

/**
 * Función que cueta las ocurrencias de un caracter en un string
 * @param s String donde se está buscando
 * @param c Caracter que se está buscando
 * @return Cantidad de ocurrencias de c en s
 * @author Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * @version 2014-09-01
 */
unsigned int str_char_count(const char *s, char c)
{
	unsigned short int count = 0, len, i;
	len = strlen(s);
	for (i=0; i<len; i++) {
		if (s[i] == c)
			count++;
	}
	return count;
}

/**
 * Función que realiza split sobre un string de acuerdo a cierto delimitador
 * @param string String donde hacer el split
 * @param delimiter Delimitador para la extracción (1 caracter)
 * @return Arreglo con las partes del string divididas por el delimitador
 * @author Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * @version 2014-09-01
 */
char **str_split(const char *string, char delimiter)
{
	char **argv, *aux, *part, *saveptr = NULL, d[2];
	unsigned short int argc, i=0;
	d[0] = delimiter; d[1] = '\0';
	aux = (char*) malloc(sizeof(char)*(strlen(string)+1));
	strcpy(aux, string);
	argc = str_char_count(aux, delimiter) + 1;
	argv = (char**) malloc(sizeof(char *)*(argc+1));
	part = strtok_r(aux, d, &saveptr);
	do {
		argv[i] = (char *) malloc(strlen(part)+1);
		strcpy(argv[i], part);
		i++;
	} while((part=strtok_r(NULL, d, &saveptr))!=NULL);
	argv[i] = NULL;
	free(aux);
	return argv;
}

/**
 * Función que permite crear el prompt que se mostrará al usuario
 * @return String con el prompt
 * @author Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * @version 2014-09-01
 */
char *shell_prompt()
{
	char *user, host[256], *cwd, *prompt, user_t;
	struct passwd *p = getpwuid(getuid());
	int prompt_size;
	user = p->pw_name;
	gethostname(host, sizeof(host));
	cwd = getcwd(NULL, 0);
	prompt_size = 7 + strlen(user) + strlen(host) + strlen(cwd);
	prompt = (char*) malloc(sizeof(char)*prompt_size);
	user_t = p->pw_uid==0 ? '#' : '$';
	snprintf(prompt, prompt_size, "[%s@%s %s]%c ", user, host, cwd, user_t);
	free(cwd);
	return prompt;
}

/**
 * Función que obtiene el string con el PATH del entorno del proceso
 * @return String con el PATH
 * @author Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * @version 2014-09-01
 */
char *shell_get_path()
{
	char *aux, *PATH;
	aux = getenv("PATH");
	PATH = (char*) malloc(sizeof(char)*strlen(aux));
	strcpy(PATH, aux);
	return PATH;
}

/**
 * Función que obtiene la ruta absoluta de un comando dado
 * @param cmd Comando que se desea obtener su ruta absoluta
 * @return String con la ruta completa del comando consultado
 * @author Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * @version 2014-09-01
 */
char *cmd_get_path(char *cmd)
{
	char *cmd_path = NULL, *PATH, *path, *saveptr = NULL;
	/* si es ruta absoluta o relativa se ve si existe directamente */
	if (str_char_count(cmd, '/')) {
		return access(cmd, F_OK) != -1 ? cmd : NULL;
	}
	/* obtener posibles rutas donde está el comando y buscar */
	PATH = shell_get_path();
	path = strtok_r(PATH, ":", &saveptr);
	if (path==NULL)
		return NULL;
	do {
		cmd_path = (char *) malloc(strlen(path)+strlen(cmd)+2);
		sprintf(cmd_path, "%s/%s", path, cmd);
		if (access(cmd_path, F_OK) != -1) {
			return cmd_path;
		} else {
			free(cmd_path);
		}
	} while((path=strtok_r(NULL, ":", &saveptr))!=NULL);
	/* si no se encontró retornar NULL */
	return NULL;
}

/**
 * Función que revisa si la ruta obtenida para un comando es válida.
 * Básicamente que sea un programa y que se pueda ejecutar. En caso de error
 * en cmd->rc se colocará un valor adecuado de código de retorno.
 * @param cmd Estructura que representa el comando a ejecutar
 * @return String con la ruta absoluta al comando o NULL en caso de error
 * @author Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * @version 2014-09-01
 */
char *cmd_check_path(struct cmd *cmd)
{
	struct stat statbuf;
	/* si no se encontró el comando mostrar mensaje de error */
	if (cmd->path == NULL) {
		fprintf(stderr, "%s: %s: no se encontró la orden\n", SHELL,
								cmd->name);
		cmd->rc = 127;
		return NULL;
	}
	/* si no hay permisos de ejecución error */
	if (access(cmd->path, X_OK) == -1) {
		fprintf(stderr, "%s: %s: permiso denegado\n", SHELL, cmd->name);
		cmd->rc = 126;
		return NULL;
	}
	/* si es un directorio error */
	stat(cmd->path, &statbuf);
	if (S_ISDIR(statbuf.st_mode)) {
		fprintf(stderr, "%s: %s: es un directorio\n", SHELL, cmd->name);
		cmd->rc = 126;
		return NULL;
	}
	return cmd->path;
}

/**
 * Función que crea la estructura necesaria para tener la representacón del cmd
 * @param input String con la entrada del usuario
 * @return Estructura con la representación del comando
 * @author Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * @version 2014-09-01
 */
struct cmd *cmd_parse(char *input)
{
	struct cmd *cmd = malloc(sizeof(struct cmd));
	/* TODO: agregar trim(input) */
	cmd->argc = str_char_count(input, ' ') + 1;
	cmd->argv = str_split(input, ' ');
	cmd->name = cmd->argv[0];
	cmd->path = NULL;
	cmd->rc = 0;
	return cmd;
}

/**
 * Función que libera la estructura que representa un comando
 * @param cmd Estructura que representa un comando
 * @author Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * @version 2014-09-01
 */
void cmd_free(struct cmd *cmd)
{
/*	int i;*/
	free(cmd->name);
/*	if (cmd->path!=NULL)
		free(cmd->path);*/
/*	for (i=0; i<cmd->argc; i++)
		free(cmd->argv[i]);*/
	free(cmd);
}

/**
 * Función que imprime un resumen con los datos del comando
 * @param cmd Estructura que representa un comando
 * @author Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * @version 2014-09-01
 */
void cmd_print(struct cmd *cmd)
{
	unsigned short int i;
	printf("=== RESUMEN COMANDO EJECUTADO ===\n");
	printf("name\t%s\n", cmd->name);
	printf("path\t%s\n", cmd->path);
	printf("argc\t%d\n", cmd->argc);
	for (i=0; i<cmd->argc; i++)
		printf("argv[%d]\t%s\n", i, cmd->argv[i]);
	printf("rc\t%d\n", cmd->rc);
	printf("=================================\n");
}

/**
 * Función que ejecuta un comando
 * @param cmd Estructura que representa un comando
 * @return Estado de salida del comando ejecutado (o error si hubo)
 * @author Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * @version 2014-09-01
 */
short int shell_exec(struct cmd *cmd)
{
	pid_t pid;
	int status;
	struct passwd *p;
	/* cambiar de directorio */
	if (!strcmp(cmd->name, "cd")) {
		if (cmd->argc==1) {
			p = getpwuid(getuid());
			return chdir(p->pw_dir);
		}
		return chdir(cmd->argv[1]);
	}
	/* obtener ruta (completa/real) del comando */
	cmd->path = cmd_get_path(cmd->name);
	if (cmd_check_path(cmd) == NULL) {
		return cmd->rc;
	}
	/* hacer fork de la shell y ejecutar */
	if ((pid = fork()) < 0) {
		fprintf(stderr, "%s: %s: error con fork()\n", SHELL, cmd->name);
		return cmd->rc;
	}
	/* si este es el proceso hijo */
	else if (pid == 0) {
		exit(execv(cmd->path, cmd->argv));
	}
	/* si este es el proceso padre */
	waitpid(pid, &status, WUNTRACED);
	if (WIFEXITED(status)!=0) {
		cmd->rc = WEXITSTATUS(status);
	} else {
		cmd->rc = EXIT_FAILURE;
	}
	return cmd->rc;
}

/**
 * Función principal de la Shell (dsh)
 * @param argc Cantidad de argumentos pasados
 * @param argv Argumentos pasados
 * @return Código de retorno al sistema operativo
 * @author Esteban De La Fuente Rubio, DeLaF (esteban[at]delaf.cl)
 * @version 2014-09-01
 */
int main(int argc, char **argv)
{
	char *prompt, *input;
	struct cmd *cmd;
	unsigned short int rc;
	while (1) {
		/* mostrar prompt y pedir entrada al usuario */
		prompt = shell_prompt();
		input = readline(prompt);
		free(prompt);
		if (!input) { /* revisar si hay EOF */
			printf("exit\n");
			rc = 0;
			break;
		}
		if (!strcmp(input, "")) {
			free(input);
			continue;
		}
		add_history(input);
		/* parsear comando que se debe ejecutar */
		cmd = cmd_parse(input);
		/* si se solicita terminar el script */
		if (!strcmp(cmd->name, "exit")) {
			rc = cmd->argc==2 ? atoi(cmd->argv[1]) : 0;
			free(input);
			cmd_free(cmd);
			break;
		}
		/* ejecutar comando solicitado con sus argumentos */
		rc = shell_exec(cmd);
		if (argc==2 && !strcmp(argv[1], "-d"))
			cmd_print(cmd);
		/* liberar memoria */
		free(input);
		cmd_free(cmd);
	}
	/* terminar la ejecución de la shell */
	return rc;
}
