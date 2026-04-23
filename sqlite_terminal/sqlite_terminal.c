/* Basic terminal interface for SQLite. https://sqlite.org/

 - Built against the sqlite amalgamation file: https://sqlite.org/amalgamation.html, but should be compatible with any sqlite3.h
 	- #include needs to be updated with the correct version of the amalgamation file.
 - Requires custom utility library utils.c/utils.h in repo.

 - Maximum length of a column name: CHAR_MAX
 - Maximum column display text length: 32
 - UTF-16 unsupported / not tested
 - Blobs unsupported

 Build instructions:
 1. Build sqlite. For the amalgamation file: gcc -o sqlite3.o -c sqlite3.c
 2. Build utils library: gcc -o utils.o -c utils.c
 3. Build sqlite_terminal.c: gcc -o sqlite_terminal sqlite_terminal.c sqlite-amalgamation-3510300/sqlite3.o utils.o -pthread -ldl
 	- Verify #include for sqlite is the correct version

 Run instructions:
 1. Sample database can be found at: https://sqlite.org/test-dbs/file?name=demo01.db (Click "Download")
 2. Run "./sqlite_terminal DB_FILE" (ex. ./sqlite_terminal demo01.db)

*/

/*
TODO: Shrink sql_command_buffer after realloc
TODO: Test UTF-16, see sqlite3_column_bytes documentation
*/

#include "sqlite-amalgamation-3510300/sqlite3.h" // -lpthread -ldl
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include "utils.h"
#include <assert.h>


int main(int argc, char** argv) {
	errno = 0;
	int main_ret = 0;
	int ret = 0;
	
	if (argc != 2) {
		UtilsError("Input database file path");
		return EXIT_FAILURE;
	}
	
	const char * const NOT_SUPPORTED_STR = "NOT SUPPORTED";
	const char * const NULL_STR = "null";
	
	// Open DB connection
	struct sqlite3 *db;
	if (sqlite3_open(argv[1], &db)) {
		UtilsError(sqlite3_errmsg(db));
		return EXIT_FAILURE;
	}
	
	const short BUFFER_CHUNK = 64;
	int curr_cmd_length = 0;
	char *sql_command_part = NULL;
	ssize_t line_length = 0;
	struct sqlite3_stmt *stmt = NULL;
	
	// MALLOCs
	
	int curr_buffer_size = BUFFER_CHUNK;
	char *sql_command_buffer = malloc(curr_buffer_size);
	if (!sql_command_buffer) {
		UtilsError("sql_command_buffer malloc");
		return EXIT_FAILURE;
	}
	sql_command_buffer[0] = '\0';
	
	size_t col_data_types_size = 16;
	char *col_data_types = malloc(col_data_types_size);
	if (!col_data_types) {
		UtilsError("col_data_types malloc");
		return EXIT_FAILURE;
	}
	
	size_t col_max_lengths_size = 16;
	char *col_max_lengths = malloc(col_max_lengths_size);
	if (!col_max_lengths) {
		UtilsError("col_max_lengths malloc");
		return EXIT_FAILURE;
	}
	
	size_t temp_result_str_size = BUFFER_CHUNK;
	char *temp_result_str = malloc(temp_result_str_size);
	if (!temp_result_str) {
		UtilsError("temp_result_str malloc");
		return EXIT_FAILURE;
	}
	size_t temp_result_str_len = 0;
	
	size_t final_result_str_size = BUFFER_CHUNK;
	char *final_result_str = malloc(final_result_str_size);
	if (!final_result_str) {
		UtilsError("final_result_str malloc");
		return EXIT_FAILURE;
	}
	size_t final_result_str_len = 0;
		
	// End MALLOCs
	
	// goto cleanup start
	
	printf("Input (\"EXIT;\" to exit): \n>");
	
	bool finish_session = false;
	do {
		// Read a statement
		bool finish_read = false;
		ssize_t num_read;
		do {
			num_read = getline(&sql_command_part, &line_length, stdin); // MALLOC sql_command_part
			if (num_read == -1) {
				UtilsError("getline");
				main_ret = EXIT_FAILURE;
				goto cleanup;
			}
			
			while ((num_read + curr_cmd_length) > curr_buffer_size) {
				if (curr_buffer_size > (INT_MAX - BUFFER_CHUNK)) {
					UtilsError("curr_buffer_size exceeds limit");
					main_ret = EXIT_FAILURE;
					goto cleanup;
				}

				curr_buffer_size += BUFFER_CHUNK;			
				if (!(sql_command_buffer = realloc(sql_command_buffer, curr_buffer_size))) {
					UtilsError("realloc");
					main_ret = EXIT_FAILURE;
					goto cleanup;
				}
			}
			strcat(sql_command_buffer, sql_command_part);
			curr_cmd_length += num_read;
			
			free(sql_command_part);
			sql_command_part = NULL;
			line_length = 0;
			
			for (int i = curr_cmd_length - 1; i >= 0; --i) {
				char c = sql_command_buffer[i];
				if (c > 20) {
					if (c == ';') {
						finish_read = true;
						break;
					} else {
						break;
					}
				}
			}
			
		} while (!finish_read);
		
		//printf("Read: %s", sql_command_buffer);
		
		if (!strncmp(sql_command_buffer, "EXIT", 4)) {
			finish_session = true;
		} else {
			// Parse statement
			if (ret = sqlite3_prepare_v2(db, sql_command_buffer, curr_cmd_length, &stmt, NULL)) {
				printf("SQLite error: %s\n", sqlite3_errstr(ret));
				
			} else {
				// Execute statement
				int step_res = sqlite3_step(stmt);
				printf("\n");
				
				if (step_res == SQLITE_DONE || step_res == SQLITE_ROW) {
					size_t new_length = 0;
					char col_val[33] = "";
				
					// See if there are results
					int num_col = sqlite3_column_count(stmt);
					if (num_col != 0) {
						// Build result string
						// Resize col_data_types
						if (col_data_types_size < num_col || col_data_types_size > num_col + 16) {
							col_data_types_size = (num_col + 16) - \
								((num_col + 16) % 16);
							if (!(col_data_types = realloc(col_data_types, col_data_types_size))) {
								UtilsError("col_data_types realloc");
								main_ret = EXIT_FAILURE;
								goto cleanup;
							}
						}
						
						// Resize col_max_lengths
						if (col_max_lengths_size < num_col || \
							col_max_lengths_size > (num_col + 16)) {
							col_max_lengths_size = ((num_col + 16) - \
								((num_col + 16) % 16));
							if (!(col_max_lengths = realloc(col_max_lengths, col_max_lengths_size))) {
								UtilsError("col_max_lengths realloc");
								main_ret = EXIT_FAILURE;
								goto cleanup;
							}	
						}
						
						temp_result_str_size = BUFFER_CHUNK;
						if (!(temp_result_str = realloc(temp_result_str, temp_result_str_size))) {
							UtilsError("temp_result_str realloc");
							main_ret = EXIT_FAILURE;
							goto cleanup;
						}
						temp_result_str[0] = '\0';
						temp_result_str_len = 0;
						
						final_result_str_size = BUFFER_CHUNK;
						if (!(final_result_str = realloc(final_result_str, final_result_str_size))) {
							UtilsError("final_result_str realloc");
							main_ret = EXIT_FAILURE;
							goto cleanup;
						}
						final_result_str[0] = '\0';
						final_result_str_len = 0;
						
						// Get result table metadata
						for (int i = 0; i < num_col; ++i) {
							col_data_types[i] = (char) sqlite3_column_type(stmt, i);
							
							const char *sq3_col_name = sqlite3_column_name(stmt, i);
							if (!sq3_col_name) {
								UtilsError("sqlite3_column_name");
								main_ret = EXIT_FAILURE;
								goto cleanup;
							}
							
							strncpy(col_val, sq3_col_name, 32);

							// Shorten column name for display purposes
							size_t col_name_len = strlen(sq3_col_name);
							if (col_name_len >= 32) {
								col_name_len = 32;
								col_val[28] = '.';
								col_val[29] = '.';
								col_val[30] = '.';
								col_val[31] = '\0';
							}
							col_max_lengths[i] = col_name_len;
							
							// + 1 for col_name_len embedded within temp_result_str as a char
							new_length = temp_result_str_len + 1 + col_name_len ;
							if (new_length >= temp_result_str_size) {
								temp_result_str_size = (new_length + BUFFER_CHUNK) - \
									((new_length + BUFFER_CHUNK) % BUFFER_CHUNK);
								if (!(temp_result_str = realloc(temp_result_str, temp_result_str_size))) {
									UtilsError("temp_result_str realloc metadata col_val");
									main_ret = EXIT_FAILURE;
									goto cleanup;
								}
							}
							// col_name_len in temp_result_str decides how many bytes to read for the col_name
							// when creating final_result_str
							assert(col_name_len > 0 && col_name_len <= 32);
							temp_result_str[temp_result_str_len] = (char) col_name_len;
							temp_result_str[temp_result_str_len + 1] = '\0';
							
							strcat(temp_result_str, col_val);
							temp_result_str_len = new_length;
						}

						if (step_res == SQLITE_ROW) {
							// Get rows
							bool no_more_rows = false;
							do {
								for (int i = 0; i < num_col; ++i) {
									const unsigned char * val_str = NULL;
									int val_bytes = 0;
									char col_type = col_data_types[i];
									if (col_type == SQLITE_INTEGER || col_type == SQLITE_FLOAT || \
										col_type == SQLITE_TEXT || col_type == SQLITE_NULL) {
										// https://sqlite.org/c3ref/column_blob.html
										const unsigned char * sq3_col_txt = \
											sqlite3_column_text(stmt, i);
										if (!sq3_col_txt) {
											ret = sqlite3_errcode(db);
											// ret is SQLITE_ROW when value is NULL
											if (ret != SQLITE_ROW) {
												UtilsError(sqlite3_errstr(ret));
												main_ret = EXIT_FAILURE;
												goto cleanup;
											
											} else {
												val_str = NULL_STR;
												val_bytes = strlen(NULL_STR);
											}
										} else {
											val_bytes = sqlite3_column_bytes(stmt, i);
											strncpy(col_val, sq3_col_txt, 32);
											
											if (val_bytes >= 32) {
												val_bytes = 32;
												col_val[29] = '.';
												col_val[30] = '.';
												col_val[31] = '.';
												col_val[32] = '\0';
											}
											val_str = col_val;
										}
										
									} else if (col_type == SQLITE_BLOB) {
										val_str = NOT_SUPPORTED_STR;
										val_bytes = strlen(NOT_SUPPORTED_STR);
									} else {
										UtilsError("col_type");
										main_ret = EXIT_FAILURE;
										goto cleanup;
									}
									assert(val_bytes <= 32);
									
									if (val_bytes > col_max_lengths[i]) {
										col_max_lengths[i] = val_bytes;
									}
									
									// + 1 for val_bytes embedded within temp_result_str
									new_length = temp_result_str_len + 1 + val_bytes;
									if (new_length >= temp_result_str_size) {
										temp_result_str_size = (new_length + BUFFER_CHUNK) - \
											((new_length + BUFFER_CHUNK) % BUFFER_CHUNK);
										if (!(temp_result_str = \
											realloc(temp_result_str, temp_result_str_size))) {
											UtilsError("temp_result_str realloc metadata");
											main_ret = EXIT_FAILURE;
											goto cleanup;
										}
									}
									// val_bytes in temp_result_str decides how many bytes to read
									// for the value when creating final_result_str
									// + 1 for when val_bytes is 0; conflicts with \0
									temp_result_str[temp_result_str_len] = (char) val_bytes + 1;
									temp_result_str[temp_result_str_len + 1] = '\0';
									
									strcat(temp_result_str, val_str);
									temp_result_str_len = new_length;
								}
								
								ret = sqlite3_step(stmt);
								switch (ret) {
									case SQLITE_DONE:
										no_more_rows = true;
										break;
									case SQLITE_ROW:
										break;
									default:
										UtilsError(sqlite3_errstr(ret));
										main_ret = EXIT_FAILURE;
										goto cleanup;
										break;
								}
							} while (!no_more_rows);
						}
						
						// Build final str
						final_result_str_size = (temp_result_str_len + BUFFER_CHUNK) - \
							((temp_result_str_len + BUFFER_CHUNK) % BUFFER_CHUNK);
						if (!(final_result_str = realloc(final_result_str, final_result_str_size))) {
							UtilsError("final_result_str realloc init");
							main_ret = EXIT_FAILURE;
							goto cleanup;
						}
						
						// Build column name row
						size_t trs_idx = 0;
						for (int i = 0; i < num_col; ++i) {
							char col_max_len = col_max_lengths[i];
							char val_len = temp_result_str[trs_idx];
							++trs_idx;
							int j = 0;
							for (; j < val_len; ++j) {
								final_result_str[final_result_str_len] = temp_result_str[trs_idx];
								++trs_idx;
								
								++final_result_str_len;
								if (!(final_result_str = realloc_if_needed(final_result_str,  \
									&final_result_str_size, &final_result_str_len, BUFFER_CHUNK))) {
									UtilsError("final_result_str realloc col name");
									main_ret = EXIT_FAILURE;
									goto cleanup;
								}
							}
							
							for (; j < col_max_len; ++j) {
								final_result_str[final_result_str_len] = ' ';
								++final_result_str_len;
								
								if (!(final_result_str = realloc_if_needed(final_result_str,  \
									&final_result_str_size, &final_result_str_len, BUFFER_CHUNK))) {
									UtilsError("final_result_str realloc col space");
									main_ret = EXIT_FAILURE;
									goto cleanup;
								}
							}
							
							final_result_str[final_result_str_len] = '|';
							++final_result_str_len;
							if (!(final_result_str = realloc_if_needed(final_result_str,  \
								&final_result_str_size, &final_result_str_len, BUFFER_CHUNK))) {
								UtilsError("final_result_str realloc col bar");
								main_ret = EXIT_FAILURE;
								goto cleanup;
							}
						}
						final_result_str[final_result_str_len] = '\n';
						++final_result_str_len;
						if (!(final_result_str = realloc_if_needed(final_result_str,  \
							&final_result_str_size, &final_result_str_len, BUFFER_CHUNK))) {
							UtilsError("final_result_str realloc col newline");
							main_ret = EXIT_FAILURE;
							goto cleanup;
						}
						
						// Build value rows
						while (trs_idx < temp_result_str_len) {
							for (int i = 0; i < num_col; ++i) {
								char col_max_len = col_max_lengths[i];
								// - 1 to undo workaround for when val_bytes was 0
								char val_len = temp_result_str[trs_idx] - 1;
								++trs_idx;
								int j = 0;
								if (val_len) {
									for (; j < val_len; ++j) {
										final_result_str[final_result_str_len] = \
											temp_result_str[trs_idx];
										++trs_idx;
										++final_result_str_len;
										
										if (!(final_result_str = realloc_if_needed( \
											final_result_str, &final_result_str_size, \
											&final_result_str_len, BUFFER_CHUNK))) {
											UtilsError("final_result_str realloc val name");
											main_ret = EXIT_FAILURE;
											goto cleanup;
										}
									}
								}
								
								for (; j < col_max_len; ++j) {
									final_result_str[final_result_str_len] = ' ';
									++final_result_str_len;
									
									if (!(final_result_str = realloc_if_needed( \
										final_result_str, &final_result_str_size, \
										&final_result_str_len, BUFFER_CHUNK))) {
										UtilsError("final_result_str realloc val space");
										main_ret = EXIT_FAILURE;
										goto cleanup;
									}
								}
								
								final_result_str[final_result_str_len] = '|';
								++final_result_str_len;
								if (!(final_result_str = realloc_if_needed( \
									final_result_str, &final_result_str_size, \
									&final_result_str_len, BUFFER_CHUNK))) {
									UtilsError("final_result_str realloc val bar");
									main_ret = EXIT_FAILURE;
									goto cleanup;
								}
							}
							final_result_str[final_result_str_len] = '\n';
							++final_result_str_len;
							
							if (!(final_result_str = realloc_if_needed( \
								final_result_str, &final_result_str_size, \
								&final_result_str_len, BUFFER_CHUNK))) {
								UtilsError("final_result_str realloc val newline");
								main_ret = EXIT_FAILURE;
								goto cleanup;
							}
						}
						
						// Terminate final_result_str
						final_result_str[final_result_str_len] = '\0';
						
						// Print
						printf("%s\n", final_result_str);
					}
					printf("Executed successfully\n");

				} else {
					printf("SQLite error: %s\n", sqlite3_errstr(step_res));

				}
			}
			sqlite3_finalize(stmt);
			stmt = NULL; // Double call of sqlite3_finalize can result in segfault
			printf(">");

		}
		curr_cmd_length = 0;
		sql_command_buffer[0] = '\0';
	
	} while (!finish_session);
	
	cleanup:
	
	if (stmt) {
		if(ret = sqlite3_finalize(stmt)) UtilsError(sqlite3_errstr(ret));
	}
	
	if (ret = sqlite3_close(db)) {
		fprintf(stderr, "cleanup sqlite3_close");
		UtilsError(sqlite3_errstr(ret));
	}
	
	if (sql_command_buffer) free(sql_command_buffer);
	sql_command_buffer = NULL;
	
	if (sql_command_part) free(sql_command_part);
	sql_command_part = NULL;
	
	if (col_data_types) free(col_data_types);
	col_data_types = NULL;
	
	if (col_max_lengths) free (col_max_lengths);
	col_max_lengths = NULL;
	
	if (temp_result_str) free(temp_result_str);
	temp_result_str = NULL;
	
	if (final_result_str) free(final_result_str);
	final_result_str = NULL;
	
	return main_ret;
}
