 Basic terminal interface for SQLite. https://sqlite.org/

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

