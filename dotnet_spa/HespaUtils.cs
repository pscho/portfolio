using System.Runtime.InteropServices;
using System.Diagnostics;

namespace HespaUtils;

internal static partial class Sqlite3 {

	// const char * return types in C don't work with a return type of string in C#
	// due to a "free() invalid pointer" issue. So I return IntPtr and just
	// do Marshal.PtrToStringUTF8 on that.
	// Related: https://github.com/dotnet/runtime/issues/76974

	[LibraryImport("sqlte_dn.so", StringMarshalling = StringMarshalling.Utf8)]
	internal static partial SqliteCode sqlite3_open(string filename, out IntPtr ppDb);
	
	[LibraryImport("sqlte_dn.so", StringMarshalling = StringMarshalling.Utf8)]
	internal static partial SqliteCode sqlite3_prepare_v2(IntPtr db, string zSql, int nByte, out IntPtr ppStmt,
													out IntPtr pzTail);
	
	[LibraryImport("sqlte_dn.so", StringMarshalling = StringMarshalling.Utf8)]
	internal static partial SqliteCode sqlite3_bind_text(
			IntPtr stmt, int i, string zData, int nData, IntPtr xDel = 0); // TODO: Delegate param for xDel?
	
	[LibraryImport("sqlte_dn.so")]
	internal static partial SqliteCode sqlite3_step(IntPtr stmt);
	
	[LibraryImport("sqlte_dn.so")]
	internal static partial SqliteCode sqlite3_finalize(IntPtr pStmt);
	
	[LibraryImport("sqlte_dn.so")]
	internal static partial SqliteCode sqlite3_close(IntPtr sqlite3);
	
	[LibraryImport("sqlte_dn.so")]
	internal static partial int sqlite3_column_count(IntPtr pStmt);
	
	[LibraryImport("sqlte_dn.so")]
	internal static partial SqliteCode sqlite3_column_type(IntPtr stmt, int iCol);
	
	[LibraryImport("sqlte_dn.so", StringMarshalling = StringMarshalling.Utf8)]
	internal static partial IntPtr sqlite3_column_name(IntPtr stmt, int N);
	
	[LibraryImport("sqlte_dn.so", StringMarshalling = StringMarshalling.Utf8)]
	internal static partial IntPtr sqlite3_column_text(IntPtr stmt, int iCol);
	
	[LibraryImport("sqlte_dn.so")]
	internal static partial SqliteCode sqlite3_column_bytes(IntPtr stmt, int iCol);
	
	[LibraryImport("sqlte_dn.so", StringMarshalling = StringMarshalling.Utf8)]
	internal static partial IntPtr sqlite3_errstr(int E);
	
	[LibraryImport("sqlte_dn.so")]
	internal static partial SqliteCode sqlite3_errcode(IntPtr db);
	
	[LibraryImport("sqlte_dn.so", StringMarshalling = StringMarshalling.Utf8)]
	internal static partial IntPtr sqlite3_errmsg(IntPtr db);
	
	[LibraryImport("sqlte_dn.so")]
	internal static partial int sqlite3_threadsafe();

	internal enum SqliteCode: int {
		SQLITE_OK = 0,
		SQLITE_ERROR = 1,
		SQLITE_INTEGER = 1,
		SQLITE_FLOAT = 2,
		SQLITE_TEXT = 3,
		SQLITE_BLOB = 4,
		SQLITE_NULL = 5,
		SQLITE_ROW = 100,
		SQLITE_DONE = 101
	}
	
	internal static void PrepareStatement(IntPtr db, string cmd, IntPtr stmt) {
		Debug.Assert(db != 0, "db");
		Debug.Assert(cmd.Length != 0, "cmd");
		Debug.Assert(stmt == 0, "stmt");

		IntPtr pzTail = new IntPtr(0);
		SqliteCode errCode = sqlite3_prepare_v2(db, cmd, -1, out stmt, out pzTail);
		if (errCode != SqliteCode.SQLITE_OK) {
			throw new Exception(String.Format("HespaUtils.PrepareStatement {0} {1} {2} {3}", errCode, db, cmd, stmt));
		}
		Debug.Assert(stmt != 0, "stmt");

		GC.KeepAlive(pzTail);	
	}

	internal static void BindText(IntPtr stmt, string val) {
		SqliteCode errCode = sqlite3_bind_text(stmt, 0, val, -1, -1);
		if (errCode != SqliteCode.SQLITE_OK) {
			throw new Exception(String.Format("HespaUtils.BindText {0} {1} {2}", errCode, stmt, val));
		}
		Debug.Assert(stmt != 0, "stmt");

	}

	internal static void RunCommand(IntPtr stmt) {
		SqliteCode errCode = SqliteCode.SQLITE_OK;
		try {
			errCode = sqlite3_step(stmt);
			if (errCode != SqliteCode.SQLITE_DONE) {
				throw new Exception(String.Format("{0}", errCode));
			}

		} catch (Exception ex) {
			Console.Error.WriteLine("HespaUtils.RunCommand {0}", stmt);
			Console.Error.WriteLine(ex.ToString());
		} finally {
			errCode = sqlite3_finalize(stmt);
			if (errCode != SqliteCode.SQLITE_OK) {
				throw new Exception(String.Format("HespaUtils.RunCommand {0} {1}", errCode, stmt));
			}
			stmt = 0;
		}

		GC.KeepAlive(stmt);
	}

	internal static List<List<string>> RunQueryNoBinding(string query, IntPtr db, IntPtr stmt) {
		Debug.Assert(query.Length != 0, "query");
		Debug.Assert(db != 0, "db");
		Debug.Assert(stmt == 0, "stmt");
		
		var rows = new List<List<string>>();
		int numCols = 0;

		PrepareStatement(db, query, stmt);
		
		SqliteCode errCode = SqliteCode.SQLITE_OK;
		SqliteCode step_res = sqlite3_step(stmt);
		if (step_res == SqliteCode.SQLITE_DONE || step_res == SqliteCode.SQLITE_ROW) {
			numCols = sqlite3_column_count(stmt);
			// See if there are results
			if (numCols != 0) {
				// Get column metadata
				var colNameRow = new List<String>(numCols);
				var colTypes = new SqliteCode[numCols];
				for (int i = 0; i < numCols; ++i) {
					IntPtr colName = sqlite3_column_name(stmt, i);
					if (colName == 0) {
						throw new Exception("sqlite3_column_name");
					}
					
					colNameRow.Add(Marshal.PtrToStringUTF8(colName)!); // Null forgiving as already checked above
					colTypes[i] = sqlite3_column_type(stmt, i);
				}
				rows.Add(colNameRow);
				
				if (step_res == SqliteCode.SQLITE_ROW) {
					// Get rows
					bool noMoreRows = false;
					do {
						var row = new List<String>(numCols);
						for (int i = 0; i < numCols; ++i) {
							switch (colTypes[i]) {
								case SqliteCode.SQLITE_NULL:
									row.Add("null");
									break;
								case SqliteCode.SQLITE_INTEGER:
								case SqliteCode.SQLITE_FLOAT:
								case SqliteCode.SQLITE_TEXT:
									IntPtr text = sqlite3_column_text(stmt, i);
									if (text == 0) {
										// errcode is SQLITE_ROW when value is a valid NULL
										errCode = sqlite3_errcode(db);
										if (errCode != SqliteCode.SQLITE_ROW) {
											throw new Exception(
												String.Format(
												"sqlite3_column_text; code: {0}", errCode));
										} else {
											row.Add("null");
										}
										
									} else {
										row.Add(Marshal.PtrToStringUTF8(text)!);
									}
									break;
								case SqliteCode.SQLITE_BLOB:
									row.Add("BLOB type not supported"); // TODO: Blobs
									break;
								default:
									throw new NotImplementedException();
							}
						}
						rows.Add(row);

						step_res = sqlite3_step(stmt);
						switch (step_res) {
							case SqliteCode.SQLITE_DONE:
								noMoreRows = true;
								break;
							case SqliteCode.SQLITE_ROW:
								break;
							default:
								throw new Exception(String.Format("sqlite3_step; code: {0}", step_res));
						}
						
					} while (!noMoreRows);
				}
			}
			
		} else {
			throw new Exception(String.Format("init sqlite3_step; code: {0}", step_res));
		}
		
		errCode = sqlite3_finalize(stmt);
		if (errCode != SqliteCode.SQLITE_OK) {
			throw new Exception(String.Format("sqlite3_finalize; errCode: {0}", errCode));
		}
		stmt = 0;

		return rows;
	} 
}


