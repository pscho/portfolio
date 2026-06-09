 Basic terminal interface for SQLite. https://sqlite.org/

 - Built against the sqlite amalgamation file: https://sqlite.org/amalgamation.html, but should be compatible with any other sqlite3.h
 - Download for the sqlite amalgamation archive containing the needed sqlite3.c and sqlite3.h files is at:
 	- https://sqlite.org/download.html
	- Please extract sqlite3.c and sqlite3.h from the compressed archive and paste them into the source directory.
 - Requires custom utility library utils.c/utils.h in repo (already included). 

 Limitations:
 - Maximum length of a column name: CHAR_MAX
 - Maximum column display text length: 32
 - UTF-16 unsupported / not tested
 - Blobs unsupported

 Build instructions:
 1. Ensure sqlite3.c and sqlite3.h have been downloaded and are in the source directory.
 2. Run `make`

 Run instructions:
 1. Sample database can be found at: https://sqlite.org/test-dbs/file?name=demo01.db (Click "Download")
 2. Run "./sqlite_terminal DB_FILE" (ex. `./sqlite_terminal demo01.db`)

