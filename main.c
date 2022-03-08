#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>

#define PART_SIZE 4096
#define NAME_MAX_LEN 255

void archive(char *dir, int outputDescriptor); //функция архивирует директорию dir, записывая результат в открытый файл с дескриптором outputDescriptor
char* unzip(int inputDescriptor, char *buf, char* currp, char *endp); //inputDescriptor - дескриптор файла архива, buf - буфер, currp - текущая позиция в этом буфере

char* readmore(int inputDescriptor, char *buf, char *currp, char *endp); //прочитает из файла в буфер следующие PART_SIZE байт, при этом оставляя в буфере ещё не обработаные данные
char* searchEntry(char *str); //ищет в строке первое вхождение символов '|' или '<', если не находит, возвращает NULL

int main(int argc, char* argv[]) {
	int input = 1; //значения по умолчанию для аргументов
	int output = 2;
	int k;
	for (k = 1; k < argc; k++) {
		if (strcmp(argv[k], "-o") == 0) { //поиск флага "-o"
			output = ++k;
			continue;
		}
		if (strcmp(argv[k], "-i") == 0) { //поиск флага "-i"
			input = ++k;
			continue;
		}
	}

	struct stat statbuf;
	lstat(argv[input], &statbuf);
	if (S_ISDIR(statbuf.st_mode)) { //если входные данные - директория, то выполняется архивация
		int outputDescriptor;
		if ((outputDescriptor = open(argv[output], O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH)) == (int) - 1) {
			printf("error opening file %s\n", argv[output]);
			return -1;
		}

		chdir(argv[input]);
		chdir("..");
		char *folderName; //короткое имя каталога, вместо пути в целом
		if ((folderName = strrchr(argv[input], '/')) == NULL) folderName = argv[input];
		else folderName++; //прибавление к указателю 1, чтобы отбросить символ "/"
		archive(folderName, outputDescriptor);
		if (close(outputDescriptor) < 0) printf("error closing file %s\n", argv[output]);
	} else { //иначе разархивация
		char *buf;
		/*буфер, в котором отведено NAME_MAX_LEN байт для запоминания имени файла и PART_SIZE байт для чтения архива по частям, а также 1 байт для '\0'*/
		if ((buf = malloc(NAME_MAX_LEN + PART_SIZE + 1)) == NULL) {
			printf("memory allocation error\n");
			return -2;
		}
		buf[0] = '\0';

		/*открытие файла архива*/
		int inputDescriptor;
		if ((inputDescriptor = open(argv[input], O_RDONLY)) < 0) {
			printf("error opening file %s\n", argv[input]);
			return -3;
		}

		if (access(argv[output], 0) != 0) {
			printf("Folder for unzip not founded\nCreate this folder\n");
			mkdir(argv[output], 0777);
		}

		chdir(argv[output]);
		unzip(inputDescriptor, buf, buf, buf);
		free(buf);
		if (close(inputDescriptor) < 0) printf("error closing file\n");
	}

	return 0;
}

void archive(char *dir, int outputDescriptor) {
	if (write(outputDescriptor, dir, strlen(dir)) != strlen(dir)) printf("Write error\n");
	if (write(outputDescriptor, "<", 1) != 1) printf("Write error\n");

	//Открытие каталога
	DIR *dirp;
	if ((dirp = opendir(dir)) == NULL) {
		printf("cannot open directory: %s\n", dir);
		return;
	}
	chdir(dir); // ЗАДАЧА 1 разобраться с необходимостью данного перехода
	// Оставляем, так как работаем с именем папки, не передавая полностью путь

	struct dirent *entry; //указатель для доступа к элементу каталога
	struct stat statbuf;
	//Цикл по элементам каталога
	while ((entry = readdir(dirp)) != NULL) {
		lstat(entry->d_name, &statbuf);
		if (S_ISDIR(statbuf.st_mode)) { //если текущий элемент - каталог, то рекурсивный вызов
			if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0) continue; //Находит каталог, но игнорирует . и ..
			archive(entry->d_name, outputDescriptor); //Рекурсивный вызов
		}
		// ЗАДАЧА 2 Реализована
		if (S_ISREG(statbuf.st_mode)) { //если текущий элемент - файл
			int fileDscr;
			if ((fileDscr = open(entry->d_name, O_RDONLY)) < 0) {
				printf("error opening file %s\n", entry->d_name);
				continue;
			}
			if (write(outputDescriptor, entry->d_name, strlen(entry->d_name)) != strlen(entry->d_name)) printf("Write error\n");
			if (write(outputDescriptor, "|", 1) != 1) printf("Write error\n");
			if (write(outputDescriptor, &(statbuf.st_size), sizeof(off_t)) != sizeof(off_t)) printf("Write error\n");
			// ЗАДАЧА 3 реализовать запись через статический буфер по частям
			// реализована
			char buf[PART_SIZE];
			off_t filesize = statbuf.st_size;
			while (filesize > PART_SIZE) { // пишем кусками через буфер пока оставшееся количество байтов больше буферов
				if (read(fileDscr, &buf, PART_SIZE) != PART_SIZE) printf("Read error file %s\n", entry->d_name);
				if (write(outputDescriptor, &buf, PART_SIZE) != PART_SIZE) printf("Write error %s\n", entry->d_name);
				filesize -= PART_SIZE;
			}
			if (filesize != 0) { // дописываем остаток
				if (read(fileDscr, &buf, filesize) != filesize) printf("Read error file %s\n", entry->d_name);
				if (write(outputDescriptor, &buf, filesize) != filesize) printf("Write error %s\n", entry->d_name);
			}
			if (close(fileDscr) < 0) printf("error closing file %s\n", entry->d_name);
		}
	}
	if (write(outputDescriptor, ">", 1) != 1) printf("Write error\n");
	chdir("..");
	closedir(dirp);
}

char* unzip(int inputDescriptor, char *buf, char* currp, char *endp) {
	char *strp;
	do {
		if ((strp = searchEntry(currp)) == NULL) {
			endp = readmore(inputDescriptor, buf, currp, endp);
			currp = buf;
		} else {
			if (strp[0] == '<') {
				strp[0] = '\0';
				mkdir(currp, S_IWUSR | S_IRUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
				chdir(currp);
				currp = unzip(inputDescriptor, buf, strp + 1, endp);
			} else {
				/*создание файла*/
				strp[0] = '\0';
				int fileDscr;
				if ((fileDscr = open(currp, O_WRONLY | O_CREAT | O_TRUNC, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH)) < 0) {
					printf("error opening file %s\n", currp);
					exit(-1);
				}
				currp = strp + 1;

				/*определение размера файла*/
				if (endp - currp < sizeof(off_t)) {
					endp = readmore(inputDescriptor, buf, currp, endp);
					currp = buf;
				}
				off_t filesize = *(off_t*)currp;
				currp = currp + sizeof(off_t);

				/*запись данных в файл*/
				while (endp - currp < filesize) {
					if (write(fileDscr, currp, endp - currp) != endp - currp) printf("Write error\n");
					filesize -= endp - currp;
					endp = readmore(inputDescriptor, buf, endp, endp);
					currp = buf;
				}
				if (write(fileDscr, currp, filesize) != filesize) printf("Write error\n");
				if (close(fileDscr) < 0) printf("error closing file");
				currp = currp + filesize;
			}
		}
	} while ((currp[0] != '>') && (buf != endp));

	chdir("..");
	return currp + 1;
}

char* readmore(int inputDescriptor, char *buf, char *currp, char *endp) {
	long int readcount;
	memmove(buf, currp, endp - currp); //сохранение в начале буфера необработанного остатка
	if ((readcount = read(inputDescriptor, buf + (endp - currp), PART_SIZE)) < 0) { //после него записывается новая прочитанная часть
		printf("Read error\n");
		exit(-1);
	}
	buf[endp - currp + readcount] = '\0';
	return buf + (endp - currp + readcount);
}

char* searchEntry(char *str) {
	int i;
	for (i = 0; (str[i] != '\0') && (str[i] != '>'); i++) {
		if ((str[i] == '|') || (str[i] == '<')) {
			return str + i;
		}
	}
	return NULL;
}

