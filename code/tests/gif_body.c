
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

#if 0
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

#define SAVE_CURRENT_FILE_POS(FP)	\
	fpos_t curPos;			\
	fgetpos(FP, &curPos)

#define RESTORE_CURRENT_FILE_POS(FP)		\
	/* Return to current position */	\
	fsetpos(FP, &curPos)

/** Ref. for uncompress() 
 *  https://commandlinefanatic.com/cgi-bin/showarticle.cgi?article=art011
 */
typedef struct {
	unsigned char byte;
	int prev;
	int len;
} dictionary_entry_t;

int uncompress(int code_length,
	       const unsigned char *input, int input_length,
	       unsigned char *out) {
	dictionary_entry_t *dict;
	int dict_idx;
	int i, bit;
	int code, prev = -1;
	unsigned int mask = 0x01;
	int reset_code_length;
	int clear_code; // This varies depending on code_length
	int stop_code;  // one more than clear code
	int match_len;

	clear_code = 1 << (code_length);
	stop_code = clear_code + 1;
	// To handle clear codes
	reset_code_length = code_length;

	// Create a dictionary large enough to hold "code_length" entries.
	// Once the dictionary overflows, code_length increases
	dict = (dictionary_entry_t *) malloc(sizeof(dictionary_entry_t) *
					     (1 << (code_length + 1))   );

	// Initialize the first 2^code_len entries of the dictionary with their
	// indices.  The rest of the entries will be built up dynamically.

	// Technically, it shouldn't be necessary to initialize the
	// dictionary.  The spec says that the encoder "should output a
	// clear code as the first code in the image data stream".  It doesn't
	// say must, though...
	for (dict_idx = 0; dict_idx < (1 << code_length); dict_idx++) {
		dict[dict_idx].byte = dict_idx;
		// XXX this only works because prev is a 32-bit int (> 12 bits)
		dict[dict_idx].prev = -1;
		dict[dict_idx].len = 1;
	}

	// 2^code_len + 1 is the special "end" code; don't give it an entry here
	dict_idx += 2;

	// TODO verify that the very last byte is clear_code + 1
	while (input_length) {
		code = 0x0;
		// Always read one more bit than the code length
		for (i = 0; i < (code_length + 1); i++) {
			// This is different than in the file read example; that
			// was a call to "next_bit"
			bit = (*input & mask) ? 1 : 0;
			mask <<= 1;

			if (mask == 0x100) {
				mask = 0x01;
				input++;
				input_length--;
			}

			code = code | (bit << i);
		}

		if (code == clear_code) {
			code_length = reset_code_length;
			dict = (dictionary_entry_t *)
				realloc(dict, sizeof(dictionary_entry_t) *
					      (1 << (code_length + 1)));

			for (dict_idx = 0; dict_idx < (1 << code_length); dict_idx++) {
				dict[dict_idx].byte = dict_idx;
				// XXX this only works because prev is a 32-bit int (> 12 bits)
				dict[dict_idx].prev = -1;
				dict[dict_idx].len = 1;
			}
			dict_idx += 2;
			prev = -1;
			continue;

		} else if (code == stop_code) {
			if (input_length > 1) {
				fprintf(stderr,
					"Malformed GIF (early stop code)\n");
				return -1;
			}
			break;
		}

		// Update the dictionary with this character plus the _entry_
		// (character or string) that came before it
		if ((prev > -1) && (code_length < 12)) {
			if (code > dict_idx) {
				fprintf(stderr, "code = %.02x, "
					"but dict_idx = %.02x\n",
					code, dict_idx);
				return -1;
			}

			// Special handling for KwKwK
			if (code == dict_idx) {
				int ptr = prev;

				while (dict[ptr].prev != -1)
					ptr = dict[ptr].prev;

				dict[dict_idx].byte = dict[ptr].byte;

			} else {
				int ptr = code;
				while (dict[ptr].prev != -1)
					ptr = dict[ptr].prev;

				dict[dict_idx].byte =
					dict[ptr].byte;
			}

			dict[dict_idx].prev = prev;

			dict[dict_idx].len = dict[prev].len + 1;

			dict_idx++;

			// GIF89a mandates that this stops at 12 bits
			if ((dict_idx == (1 << (code_length + 1))) &&
			    (code_length < 11)) {
				code_length++;

				dict = (dictionary_entry_t *)
					realloc(dict, sizeof(dictionary_entry_t) *
						      (1 << (code_length + 1)));
			}
		}

		prev = code;

		// Now copy the dictionary entry backwards into "out"
		match_len = dict[code].len;
		while (code != -1) {
			out[dict[code].len - 1] = dict[code].byte;
			if (dict[code].prev == code) {
				fprintf(stderr, "Internal error; "
					"self-reference.");
				return -1;
			}
			code = dict[code].prev;
		}

		out += match_len;
	}
	return 0;
}

int main(int argc, char **argv) {
	FILE *fp;
	char *fPath;
	int i = 0, LINE_LIMIT = DEFAULT_LINE_LIMIT;
	int wFrame, hFrame, northPos, westPos;
	int twoSPwr[2], ctEntry[2];
	unsigned char c, prevC;
	unsigned char hasGct, hasLct;
	unsigned char extCode, nGceDatas, codeLen;
	int totalBytesOfDatas = 0, nSubBlocks = 0;
	int rc;

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
	printf("%s>>> Skipping header ...%s\n", CYAN(BOLD_CLR), NC);
	rc = fseek(fp, 6, SEEK_SET);
	if (rc) {
		LOG_ERR("fseek failed...\n");
		fclose(fp);
		return 0;
	}

	printf("%s>>> Skipping size ...%s\n", CYAN(BOLD_CLR), NC);
	rc = fseek(fp, 4, SEEK_CUR);
	if (rc) {
		LOG_ERR("fseek failed...\n");
		fclose(fp);
		return 0;
	}

	printf("%s>>> Has GCT ?%s ", CYAN(BOLD_CLR), NC);
	c = (unsigned char) fgetc(fp);
	twoSPwr[0] = (c & 7) + 1;
	hasGct  = c & (1 << 7);
	(void) fgetc(fp);
	(void) fgetc(fp);

	if (hasGct) {
		printf("Yes\n");
		printf("%s'-> Skipping it...%s\n", CYAN(BOLD_CLR), NC);

		ctEntry[0] = 1 << twoSPwr[0];

		rc = fseek(fp, 3*ctEntry[0], SEEK_CUR);
		if (rc) {
			LOG_ERR("fseek failed...\n");
			fclose(fp);
			return 0;
		}
	} else {	printf("No\n");	}

	printf("%s>>> GCE:%s\n", CYAN(BOLD_CLR), NC);
	c = fgetc(fp);
	if (c != '!') {
		LOG_ERR("'-> Start symbol not as expected (0x%02X, "
			"got 0x%02X)\n", c, '!');
		fclose(fp);
		return -1;
	}
	printf("%s'->%s Start symbol as expected!\n", CYAN(BOLD_CLR), NC);

	extCode = fgetc(fp);
	if (extCode != 0xF9 && extCode != 0xFF) {
		LOG_ERR("'-> Extension code not recognized\n");
		fclose(fp);
		return -1;
	}
	printf("%s'->%s Extension code: 0x%02X (%s)\n", CYAN(BOLD_CLR), NC,
	       extCode, extCode == 0xF9 ? "Simple Frame" : "Animation");

	nGceDatas = fgetc(fp);
	printf("%s'->%s # of GCE Datas: %d\n", CYAN(BOLD_CLR), NC, nGceDatas);
	if (extCode == 0xF9) {	/* Simple frame */
		c = fgetc(fp);
		printf("%s'->%s Has transparency: %s\n", CYAN(BOLD_CLR), NC,
		       c & (1 << 0) ? "Yes" : "No");

		(void) fgetc(fp);	/* Skip frame delay, as it is */
		(void) fgetc(fp);	/* useless on single frame    */
		printf("%s'->%s Frame delay: Don't care\n", CYAN(BOLD_CLR), NC);

		c = fgetc(fp);
		printf("%s'->%s Transparent col. idx: %d\n",
		       CYAN(BOLD_CLR), NC, c);

		if ((c = fgetc(fp), c) != 0) {
			LOG_ERR("'-> End symbol not as expected (0x%02X, "
				"got 0x%02X)\n", c, 0);
			fclose(fp);
			return -1;
		}
		printf("%s'->%s End symbol as expected!\n",
		       CYAN(BOLD_SET), NC);

		printf("%s>>> Image descriptor:%s\n", CYAN(BOLD_CLR), NC);
		c = fgetc(fp);
		if (c != ',') {
			LOG_ERR("'-> Start symbol not as expected (0x%02X, "
				"got 0x%02X)\n", c, ',');
			fclose(fp);
			return -1;
		}
		printf("%s'->%s Start symbol as expected!\n",
		       CYAN(BOLD_CLR), NC);

		westPos   = fgetc(fp);
		westPos  += (fgetc(fp) * 256);
		northPos  = fgetc(fp);
		northPos += (fgetc(fp) * 256);
		printf("%s'->%s North/West corner pos: (%dx%d)\n",
		       CYAN(BOLD_CLR), NC, westPos, northPos);

		wFrame  = fgetc(fp);
		wFrame += (fgetc(fp) * 256);
		hFrame  = fgetc(fp);
		hFrame += (fgetc(fp) * 256);
		printf("%s'->%s Frame's size: %dx%d\n",
		       CYAN(BOLD_CLR), NC, wFrame, hFrame);

		c = fgetc(fp);
		printf("%s'-> LCT: %s0x%02X\n", CYAN(BOLD_CLR), NC, c);
		twoSPwr[1] = (c & 7) + 1;
		printf("%s   |-> %sCT # entry : 2^%d\n", CYAN(BOLD_CLR), NC,
		       twoSPwr[1]);
		hasLct = c & (1 << 7);
		printf("%s   %c-> %sHas LCT    : %s\n", CYAN(BOLD_CLR), 
		       hasLct ? '|' : '\'', NC, hasLct ? "Yes" : "No");

		if (hasLct) {
			ctEntry[1] = 1 << twoSPwr[1];
			printf("%s   '-> %s  R  |  G  |  B  \n",
			       CYAN(BOLD_CLR), NC);
			printf("        --- | --- | ---\n");
			for (i = 0; i < ctEntry[1]; i++) {
				c = getc(fp);
				printf("        %3d | ", c);
				c = getc(fp);
				printf("%3d | ", c);
				c = getc(fp);
				printf("%3d\n", c);
			}
		}

		printf("%s>>> Image datas:%s\n", CYAN(BOLD_CLR), NC);
		codeLen = fgetc(fp);
		printf("%s'->%s LZW Min. code size: %d\n",
		       CYAN(BOLD_CLR), NC, codeLen);

		SAVE_CURRENT_FILE_POS(fp);

		/* Count Image data sub-blocks */
		do {
			nSubBlocks++;

			c = fgetc(fp);
			totalBytesOfDatas += c;
			printf("%s'->%s Sub-block size    : %3d\n",
			       CYAN(BOLD_CLR), NC, c);

			if (c == 255) {
				rc = fseek(fp, c, SEEK_CUR);
				if (rc) {
					LOG_ERR("fseek failed...\n");
					fclose(fp);
					return 0;
				}
			}
		} while (c == 255);

		RESTORE_CURRENT_FILE_POS(fp);

		DBG_INF("Full length of Data bytes: %d, # of Sub-blocks: %d\n",
			totalBytesOfDatas, nSubBlocks);

		/* TODO: Find a way to use uncompress on big pics */
		//if (totalBytesOfDatas > 255)	goto remaining_datas;

		unsigned char *readData = (unsigned char *)
					  malloc(totalBytesOfDatas);
		if ( ! readData ) {
			LOG_ERR("Could not allocate readData...\n");
			fclose(fp);
			return -1;
		}

		unsigned char *uncompressed = (unsigned char *)
					      malloc(wFrame * hFrame);
		if ( ! uncompressed ) {
			LOG_ERR("Could not allocate uncompressed...\n");
			free(readData);
			fclose(fp);
			return -1;
		}

		prevC = 0;
		for (i = 0; i < nSubBlocks; i++) {
			c = fgetc(fp);	/* Read sub-block size */

			/* Actually read datas and
			 * store them after previous batch */
			fread(readData + prevC, 1, c, fp);

			prevC = c;
			DBG_INF("i[%d]: prevC = %d\n", i, prevC);
		}

		rc = uncompress(codeLen, readData, totalBytesOfDatas, uncompressed);
		if (rc) {
			LOG_ERR("uncompress failed...\n");
			free(readData);
			free(uncompressed);
			fclose(fp);
			return -1;
		}

		for (i = 0; i < wFrame * hFrame; i++)
			printf("0x%02X%c", uncompressed[i],
			       ((i+1) % LINE_LIMIT) ? ' ' : '\n');
		printf("%s", ((i+1) % LINE_LIMIT) ? "\n" : "");
		/*for (i = 0; i < wFrame * hFrame; i++)
			printf("%c%s", uncompressed[i] ? '0' : ' ',
			       ((i+1) % wFrame) ? "" : "\n");*/

		free(readData);
		free(uncompressed);
	} else {		/* Animation */
	}

remaining_datas:
	printf("%s>>> Remaining datas ...%s\n", CYAN(BOLD_CLR), NC);
	i = 0;
	do {
		prevC = c;
		c = fgetc(fp);
		i++;
		i %= LINE_LIMIT;
		printf("0x%02X%c", c, i ? ' ' : '\n');
	} while (prevC != 0 || c != ';');

	DBG_INF("%sData bytes / Remaining bytes: (%d/%d)%s",
		i ? "\n" : "", totalBytesOfDatas, i, i ? "" : "\n");

	printf("%s%s--- Stop reading file ---%s\n",
	       i ? "\n" : "", GREEN(BOLD_CLR), NC);
	fclose(fp);
	return 0;
}
