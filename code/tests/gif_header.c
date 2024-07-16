
#include <stdio.h>
#include <stdlib.h>

#define BOLD_CLR	0
#define BOLD_SET	1

#define VAL_TO_STR(VAL)		#VAL
#define DEFINE_TO_STR(DEF)	VAL_TO_STR(DEF)

#define CYAN(BOLD)	"\033[" DEFINE_TO_STR(BOLD) ";36m"
#define PURPLE(BOLD)	"\033[" DEFINE_TO_STR(BOLD) ";35m"
#define BLUE(BOLD)	"\033[" DEFINE_TO_STR(BOLD) ";34m"
#define YELLOW(BOLD)	"\033[" DEFINE_TO_STR(BOLD) ";33m"
#define GREEN(BOLD)	"\033[" DEFINE_TO_STR(BOLD) ";32m"
#define RED(BOLD)	"\033[" DEFINE_TO_STR(BOLD) ";31m"
#define NC		"\033[0m"

#define LOG_ERR(FMT, ...)				\
	do {						\
		printf("[%s] " RED(BOLD_SET) FMT NC,	\
		       __func__, ##__VA_ARGS__);	\
	} while(0)
#define LOG_INF(FMT, ...)				\
	do {						\
		printf("[%s] " YELLOW(BOLD_CLR) FMT NC,	\
		       __func__, ##__VA_ARGS__);	\
	} while(0)

#if 1
#define DBG_ERR(FMT, ...)				\
	do {						\
		printf("[%s] " RED(BOLD_SET) FMT NC,	\
		       __func__, ##__VA_ARGS__);	\
	} while(0)
#define DBG_INF(FMT, ...)				\
	do {						\
		printf("[%s] " YELLOW(BOLD_CLR) FMT NC,	\
		       __func__, ##__VA_ARGS__);	\
	} while(0)
#else
#define DBG_ERR(FMT, ...)
#define DBG_INF(FMT, ...)
#endif

#define DEFAULT_LINE_LIMIT	10

int main(int argc, char **argv) {
	FILE *fp;
	char *fPath;
	int i = 0, LINE_LIMIT = DEFAULT_LINE_LIMIT;
	int w, h, hasGct;
	int twoSPwr, ctEntry;
	unsigned char c, prevC;

	if (argc < 2 || argc > 3) {
		LOG_ERR("Wrong number of arguments!\n");
		printf("usage: %s <file_to_read> [byte_lim_per_line]\n", argv[0]);
		return -1;

	} else if (argc == 3) {
		LINE_LIMIT = atoi(argv[2]);
		if (LINE_LIMIT <= 0) {
			LOG_INF("byte_lim_per_line must be > 0! "
				"Value has been kept to default (%d)\n",
				DEFAULT_LINE_LIMIT);
			LINE_LIMIT = DEFAULT_LINE_LIMIT;
		}
	}
	
	fPath = argv[1];
	fp = fopen(fPath, "r");

	if ( ! fp ) {
		LOG_ERR("fopen failed on \"%s\"!\n", fPath);
		return -1;
	}

	printf("%s--- Start reading file ---%s\n", GREEN(BOLD_CLR), NC);
	printf("%s>>> Header: %s", CYAN(BOLD_CLR), NC);
	do {
		c = fgetc(fp);
		printf("%c", c);
	} while (++i < 6);
	printf("\n");

	printf("%s>>> Size: %s", CYAN(BOLD_CLR), NC);
	w  = fgetc(fp);
	w += (fgetc(fp) * 256);
	h  = fgetc(fp);
	h += (fgetc(fp) * 256);
	printf("%dx%d\n", w, h);

	c = (unsigned char) fgetc(fp);
	printf("%s>>> GCT: %s0x%02X\n", CYAN(BOLD_CLR), NC, c);
	twoSPwr = (c & 7) + 1;
	printf("%s|-> %sCT # entry : 2^%d\n", CYAN(BOLD_CLR), NC, twoSPwr);
	hasGct = c & (1 << 7);
	printf("%s|-> %sHas GCT    : %s\n", CYAN(BOLD_CLR), NC,
	       hasGct ? "Yes" : "No");
	c = (unsigned char) fgetc(fp);
	printf("%s|-> %sBack. index: 0x%02X\n", CYAN(BOLD_CLR), NC, c);
	c = (unsigned char) fgetc(fp);
	printf("%s%c-> %sPx asp. ra.: 0x%02X\n", CYAN(BOLD_CLR), 
	       hasGct ? '|' : '\'', NC, c);

	if (hasGct) {
		ctEntry = 1 << twoSPwr; 
		printf("%s'-> %s  R  |  G  |  B  \n", CYAN(BOLD_CLR), NC);
		printf("     --- | --- | ---\n");
		for (i = 0; i < ctEntry; i++) {
			c = (unsigned char) getc(fp);
			printf("     %3d | ", c);
			c = (unsigned char) getc(fp);
			printf("%3d | ", c);
			c = (unsigned char) getc(fp);
			printf("%3d\n", c);
		}
	}

	printf("%s>>> Remaining datas ...%s\n", CYAN(BOLD_CLR), NC);
	i = 0;
	do {
		prevC = c;
		c = fgetc(fp);
		i++;
		i %= LINE_LIMIT;
		printf("0x%02X%c", c, i ? ' ' : '\n');
	} while (prevC != 0 || c != ';');

	printf("%s%s--- Stop reading file ---%s\n",
	       i ? "\n" : "", GREEN(BOLD_CLR), NC);
	fclose(fp);
	return 0;
}
