using System.Net.Sockets;
using System.Net;
using System.Threading;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Text.Json;
using static HespaUtils.Sqlite3;

namespace NorthwindServer;

// TODO: CTRL + C interrupt

// Sqlite:
// - Need -shared flag for gcc
// - LibraryImportAttribute requires output file have ".so" as file extension
// - Consider WAL mode as an optimization option
//		https://news.ycombinator.com/item?id=33975635
//		https://sqlite.org/wal.html

static class SPAServer {
	
	internal static bool Stop = false;
	internal static string DatabaseFilePath = "";
	
	static void Main(string[] args) {
		const int PENDING_CONN_QUEUE_SIZE = 100;
		
		// Validate args
		if (args.Length != 3) {
			Console.Error.WriteLine("3 arguments required: [databaseFilePath] [address] [port]");
			return;
		}

		var addrStr = args[1];
		string[] addrParts = addrStr.Split('.');
		bool addrValidated = false;
		Int64 addr = 0;
		if (addrParts.Length == 4) {
			try {
				addr += Int64.Parse(addrParts[3]) * 16 * 16 * 16 * 16 * 16 * 16;
				addr += Int64.Parse(addrParts[2]) * 16 * 16 * 16 * 16;
				addr += Int64.Parse(addrParts[1]) * 16 * 16;
				addr += Int64.Parse(addrParts[0]);
				addrValidated = true;
			} catch (Exception ex) {
				Console.Error.WriteLine(ex.Message);
				Console.Error.WriteLine(ex.StackTrace);
			}	
		}

		if (!addrValidated) {
			Console.Error.WriteLine("Invalid address: {0}", addrStr);
			return;
		}

		int port = 0;
		try {
			port = Int32.Parse(args[2]);
		} catch (Exception ex) {
			Console.Error.WriteLine(ex.Message);
			Console.Error.WriteLine(ex.StackTrace);
			Console.Error.WriteLine("Invalid port: {0}", args[2]);
			return;
		}

		var fInfo = new FileInfo(args[0]);
		if (!fInfo.Exists) {
			Console.Error.WriteLine("File not found; file argument: {0}", args[0]);
			return;
		}
		DatabaseFilePath = args[0];
		
		// Max allowed for untrusted code is 1 MB
		// https://learn.microsoft.com/en-us/dotnet/api/system.threading.thread.-ctor
		const int THREAD_STACK_SIZE = 1024 * 512;
	
		Console.WriteLine("Starting SPAServer...");
		
		var socketThreads = new List<Thread>();		
		var socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
		try {
			var endpoint = new IPEndPoint(new IPAddress(addr), port);
			socket.Bind(endpoint);
			socket.Listen(PENDING_CONN_QUEUE_SIZE);
		
			int threadIdCounter = 0;	
			//bool stop = true;
			while (!Stop) {
				var childSocket = socket.Accept();
				var newProcessor = new ChildSocketProcessor(threadIdCounter, childSocket);
				++threadIdCounter;
				var newThread = new Thread(newProcessor.Run, THREAD_STACK_SIZE);
				newThread.Start();
				socketThreads.Add(newThread);
			}
		
		} catch (Exception ex) {
			Console.Error.WriteLine(ex.Message);
			Console.Error.WriteLine(ex.StackTrace);
			
		} finally {
			try {
				Console.WriteLine("Shutting down listening socket...");
				socket.Shutdown(SocketShutdown.Both);
			} catch (Exception ex) {
				Console.Error.WriteLine(ex.Message);
				Console.Error.WriteLine(ex.StackTrace);
			}
			socket.Close();
		}
		
		foreach (Thread thread in socketThreads) {
			Console.WriteLine("Joining thread...");
			thread.Join();
		}
	}
}

class ChildSocketProcessor {
	const Int32 BUFFER_SIZE = 512;
	const string RESP_OK = "HTTP/1.1 200 OK";
	const string responseTemplate = "{0}\r\n{1}\r\n\r\n{2}";
	
	int Id;
	Socket ChildSocket;
	
	internal ChildSocketProcessor(int id, Socket childSocket) {
		Id = id;
		ChildSocket = childSocket;
		childSocket.SendTimeout = 0;
		childSocket.ReceiveTimeout = 30000;
	}

	private enum RequestMsg {
		BAD_REQUEST,
		UNPROCESSIBLE_CONTENT,
		MAIN,
		JS,
		GET_ORDERS,
		GET_CUSTOMERS,
		GET_SUPPLIERS,
		GET_SHIPPERS,
		GET_PRODUCTS,
		GET_CATEGORIES,
		GET_EMPLOYEES,
		POST_ORDERS,
		POST_CUSTOMERS,
		POST_SUPPLIERS,
		POST_SHIPPERS,
		POST_PRODUCTS,
		POST_CATEGORIES,
		POST_EMPLOYEES
	}
	
	private enum ReqParse {
		Method,
		Url,
		Protocol,
		FinishProtocol,
		EmptyLine_CR,
		FieldName,
		Error,
		Body,
		FinishFieldName,
		FieldValue,
		FieldValueSpace,
		FieldValue_CR,
		FinishFieldValue
	}

	internal void Run() {
		IntPtr db = new IntPtr(0);
		IntPtr dbStmt = new IntPtr(0);
		string requestStr = "";
	
		try {
			Console.WriteLine("{0} Thread {1} starting...", System.DateTime.Now.ToString(), Id);
			
			// Hard-failing ASCII encoder
			var ascii = Encoding.GetEncoding("us-ascii",
								new EncoderExceptionFallback(),
								new DecoderExceptionFallback());
			var utf8 = Encoding.UTF8;
			var utf16 = Encoding.Unicode;
			
			var requestBytes = new List<Byte>(1024);
			var responseBytes = new List<Byte>(1024);
			Byte[] buffer = new Byte[BUFFER_SIZE];
			SocketError socketErr = SocketError.SocketError;
			
			bool stop = false;
			while (!SPAServer.Stop && !stop) {
				requestBytes.Clear();
				requestBytes.Capacity = 1024;
				requestStr = "";
			
				// Receive request
				Int32 numRead = ChildSocket.Receive(buffer, 0, BUFFER_SIZE, SocketFlags.None,
					out socketErr);
				if (socketErr == SocketError.Success) {
					if (numRead > 0) {
						for (int i = 0; i < numRead; ++i) {
							requestBytes.Add(buffer[i]);
						}
					}
					
				} else {
					Console.Error.WriteLine("{0} Thread {1}: ChildSocket.Receive error: {2}",
						System.DateTime.Now.ToString(), Id, socketErr);
					stop = true;
				}
				
				if (!stop) {
					var dateTimePattern = System.Globalization.CultureInfo.
								InvariantCulture.DateTimeFormat.RFC1123Pattern;
					string dateTimeStr;
					string responseStr = "";

					responseBytes.Clear();
					bool closeConnection = false;	
					try {
						bool asciiValidated = false;
						RequestMsg requestMsg = RequestMsg.BAD_REQUEST;
						try {
							requestStr = ascii.GetString(requestBytes.ToArray(), 0, requestBytes.Count);
							asciiValidated = true;
						} catch (Exception ex) {
							string byteStr = "";
							for (int i = 0; i < requestBytes.Count; ++i) {
								byteStr += requestBytes[i].ToString() + " ";
							}
							
							Console.Error.WriteLine("{0} Thread {1}: request encoding error: {2}\r\n" +
								"Request (bytes): {3}",
								System.DateTime.Now.ToString(), Id, ex.Message, byteStr);
						}
						
						if (asciiValidated) {
							Console.WriteLine("{0} Thread {1}: Received:\n{2}", System.DateTime.Now.ToString(),
												 Id, requestStr);
							
							/*const string VALID_FIRST_GET = "GET / HTTP/1.1\r\n";
							
							char[] requestChars = new char[requestStr.Length];
							for (int i = 0; i < requestStr.Length; ++i) {
								requestChars[i] = requestStr[i];
								if (i == VALID_FIRST_GET.Length - 1) {
									var toStr = new String(requestChars).TrimEnd('\0');
									if (toStr.Equals(VALID_FIRST_GET)) {
										requestMsg = 0;
										break;
									}
								}
							}*/
							
							string httpMethod = "";
							string url = "";
							string protocol = "";
							var reqHeaders = new Dictionary<string, string>();
							string reqBody = ""; // TODO: Pick up here... parse headers and body, POST requests
							
							// Headers parsed according to RFC 7230:
							// https://www.rfc-editor.org/info/rfc7230/#section-3.2
							ReqParse parseState = ReqParse.Method;
							string fieldName = "";
							string fieldValue = "";
							for (int i = 0; i < requestBytes.Count; ++i) {
								byte c = requestBytes[i];
								switch (parseState) {
									case ReqParse.Method:
										if (c == 32) {
											parseState = ReqParse.Url;
										} else {
											httpMethod += c;
										}
										break;
									case ReqParse.Url:
										if (c == 32) {
											parseState = ReqParse.Protocol;
										} else {
											url += c;
										}
										break;
									case ReqParse.Protocol:
										protocol += c;
										if (c == 10) {
											parseState = ReqParse.FinishProtocol;
										}
										break;
									case ReqParse.FinishProtocol:
										if (c == 13) {
											parseState = ReqParse.EmptyLine_CR;
										} else if (c >= 33 && c <= 126) {
											fieldName += c;
											parseState = ReqParse.FieldName;
										} else {
											parseState = ReqParse.Error;
										}
										break;
									case ReqParse.EmptyLine_CR:
										if (c == 10) {
											parseState = ReqParse.Body;
										} else {
											parseState = ReqParse.Error;
										}
										break;
									case ReqParse.Body:
										reqBody += c;
										break;
									case ReqParse.FieldName:
										if (c >= 33 && c <= 126) {
											fieldName += c;
										} else if (c == 58) {
											parseState = ReqParse.FinishFieldName;
										} else {
											parseState = ReqParse.Error;
										}
										break;
									case ReqParse.FinishFieldName: 
										if (c == 32) { 
											;
										} else if (c >= 33 && c <= 126) {
											fieldValue += c;
											parseState = ReqParse.FieldValue;
										} else {
											parseState = ReqParse.Error;
										}
										break;
									case ReqParse.FieldValue: 
										if (c >= 33 && c <= 126) {
											fieldValue += c;
										} else if (c == 32) {
											fieldValue += c;
											parseState = ReqParse.FieldValueSpace;
										} else if (c == 13) {
											parseState = ReqParse.FieldValue_CR;
										} else {
											parseState = ReqParse.Error;
										}
										break;
									case ReqParse.FieldValue_CR:
										if (c == 10) {
											if (reqHeaders.ContainsKey(fieldName)) {
												parseState = ReqParse.Error;
											} else {
												reqHeaders.Add(fieldName, fieldValue);
												fieldName = "";
												fieldValue = "";
												parseState = ReqParse.FinishFieldValue;
											}
										} else {
											parseState = ReqParse.Error;
										}
										break;
									case ReqParse.FieldValueSpace:
										if (c >= 33 && c <= 126) {
											fieldValue += c;
											parseState = ReqParse.FieldValue;
										} else if (c == 32) {
											fieldValue += c;
										} else {
											parseState = ReqParse.Error;
										}
										break;
									case ReqParse.FinishFieldValue:
										if (c >= 33 && c <= 126) {
											fieldName += c;
											parseState = ReqParse.FieldName;
										} else if (c == 13) {
											parseState = ReqParse.EmptyLine_CR;
										} else {
											parseState = ReqParse.Error;
										}
										break;
								}
							}

							if (parseState != ReqParse.Body)
								parseState = ReqParse.Error;

							object? parsedBody = null;
							if (parseState != ReqParse.Error && string.Equals(protocol, "HTTP/1.1\r\n")) {
								switch (httpMethod) {
									case "GET":
										switch (url) {
											case "/":
												requestMsg = RequestMsg.MAIN;
												break;
											case "/spa.js":
												requestMsg = RequestMsg.JS;
												break;
											case "/orders":
												requestMsg = RequestMsg.GET_ORDERS;
												break;
											case "/customers":
												requestMsg = RequestMsg.GET_CUSTOMERS;
												break;
											case "/suppliers":
												requestMsg = RequestMsg.GET_SUPPLIERS;
												break;
											case "/shippers":
												requestMsg = RequestMsg.GET_SHIPPERS;
												break;
											case "/products":
												requestMsg = RequestMsg.GET_PRODUCTS;
												break;
											case "/categories":
												requestMsg = RequestMsg.GET_CATEGORIES;
												break;
											case "/employees":
												requestMsg = RequestMsg.GET_EMPLOYEES;
												break;
										}
										break;
									
									case "POST":
										switch (url) {
											case "/orders":
												requestMsg = RequestMsg.POST_ORDERS;
												try {
													parsedBody = 
														JsonSerializer.Deserialize
														<Order>(reqBody);
												} catch (JsonException) {
													requestMsg = RequestMsg.
														UNPROCESSIBLE_CONTENT;
												}
												break;
											case "/customers":
												requestMsg = RequestMsg.POST_CUSTOMERS;
												try {
													parsedBody = 
														JsonSerializer.Deserialize
														<Customer>(reqBody);
												} catch (JsonException) {
													requestMsg = RequestMsg.
														UNPROCESSIBLE_CONTENT;
												}
												break;
											case "/suppliers":
												requestMsg = RequestMsg.POST_SUPPLIERS;
												try {
													parsedBody = 
														JsonSerializer.Deserialize
														<Supplier>(reqBody);
												} catch (JsonException) {
													requestMsg = RequestMsg.
														UNPROCESSIBLE_CONTENT;
												}
												break;
											case "/shippers":
												requestMsg = RequestMsg.POST_SHIPPERS;
												try {
													parsedBody = 
														JsonSerializer.Deserialize
														<Shipper>(reqBody);
												} catch (JsonException) {
													requestMsg = RequestMsg.
														UNPROCESSIBLE_CONTENT;
												}
												break;
											case "/products":
												requestMsg = RequestMsg.POST_PRODUCTS;
												try {
													parsedBody = 
														JsonSerializer.Deserialize
														<Product>(reqBody);
												} catch (JsonException) {
													requestMsg = RequestMsg.
														UNPROCESSIBLE_CONTENT;
												}
												break;
											case "/categories":
												requestMsg = RequestMsg.POST_CATEGORIES;
												try {
													parsedBody = 
														JsonSerializer.Deserialize
														<Category>(reqBody);
												} catch (JsonException) {
													requestMsg = RequestMsg.
														UNPROCESSIBLE_CONTENT;
												}
												break;
											case "/employees":
												requestMsg = RequestMsg.POST_EMPLOYEES;
												try {
													parsedBody = 
														JsonSerializer.Deserialize
														<Employee>(reqBody);
												} catch (JsonException) {
													requestMsg = RequestMsg.
														UNPROCESSIBLE_CONTENT;
												}
												break;
										}
										break;
								}
							}
						}
						
						// Response
						string body = "";
						string headersTemplate = "";
						Byte[] bodyUtf8Bytes;
						string headersStr;
						string query = "";
						switch (requestMsg) {
							case RequestMsg.BAD_REQUEST:
							case RequestMsg.UNPROCESSIBLE_CONTENT:
								string codeStr;
								if (requestMsg == RequestMsg.BAD_REQUEST) {
									codeStr = "400 BAD REQUEST";
								} else {
									codeStr = "422 UNPROCESSIBLE CONTENT";
								}

								dateTimeStr = System.DateTime.Now.ToString(dateTimePattern);
								responseStr = 
									"HTTP/1.1 {0}\r\nDate: {1}\r\nContent-Length: 0\r\n\r\n";
								responseStr = String.Format(responseStr, codeStr, dateTimeStr);
								closeConnection = true;
								break;
								
							case RequestMsg.MAIN:
							case RequestMsg.JS:
								/*const string body = "<!DOCTYPE html>\r\n" +
									"<html>\r\n" + 
									"<head>\r\n" +
									"<title>Hello, World!</title>\r\n" +
									"</head>\r\n" +
									"<body>\r\n" +
									"Hello, world!\r\n" +
									"</body>\r\n" +
									"</html>";*/

								string filePath = "";
								string contentType = "";
								switch (requestMsg) {
									case RequestMsg.MAIN:
										filePath = "spa.html";
										contentType = "text/html";
										break;
									case RequestMsg.JS:
										filePath = "spa.js";
										contentType = "text/javascript";
										break;
								}
								Debug.Assert(filePath.Length > 0 && contentType.Length > 0,
											 "filePath contentType not empty");

								using (StreamReader sr = File.OpenText(filePath))
								{
									body = sr.ReadToEnd();
								}
								
								headersTemplate = "Content-Type: {0}; charset=utf-8\r\n" +
									"Content-Length: {1}\r\n" +
									"Date: {2}";
									
								bodyUtf8Bytes = Encoding.Convert(utf16, utf8, utf16.GetBytes(body));
								dateTimeStr = System.DateTime.Now.ToString(dateTimePattern);
								headersStr = String.Format(headersTemplate,
													contentType,
													bodyUtf8Bytes.Length,
													dateTimeStr);
								responseStr = String.Format(responseTemplate,
												RESP_OK,
												headersStr,
												body);
								break;
								
							case RequestMsg.GET_ORDERS:
							case RequestMsg.GET_CUSTOMERS:
							case RequestMsg.GET_SUPPLIERS:
							case RequestMsg.GET_SHIPPERS:
							case RequestMsg.GET_PRODUCTS:
							case RequestMsg.GET_CATEGORIES:
							case RequestMsg.GET_EMPLOYEES:
								// Establish initial DB connection
								if (db == 0) {
									SqliteCode errCode = sqlite3_open(SPAServer.DatabaseFilePath, out db);
									if (errCode != SqliteCode.SQLITE_OK) {
										throw new Exception(String.Format(
											"sqlite3_open; errCode: {0}", errCode));
									}
									Debug.Assert(db != 0, "sqlite3_open");
									
									// sqlite3_threadsafe needs to return 1 for thread safety
									int retVal = sqlite3_threadsafe();
									if (retVal != 1) {
										throw new Exception(
											String.Format("sqlite3_threadsafe; {0}", retVal));
									}									
								}
								
								switch (requestMsg) {
									case RequestMsg.GET_ORDERS:
										query = "SELECT * FROM Orders;";
										break;
									case RequestMsg.GET_CUSTOMERS:
										query = "SELECT * FROM Customers;";
										break;
									case RequestMsg.GET_SUPPLIERS:
										query = "SELECT * FROM Suppliers;";
										break;
									case RequestMsg.GET_SHIPPERS:
										query = "SELECT * FROM Shippers;";
										break;
									case RequestMsg.GET_PRODUCTS:
										query = "SELECT * FROM Products;";
										break;
									case RequestMsg.GET_CATEGORIES:
										query = "SELECT * FROM Categories;";
										break;
									case RequestMsg.GET_EMPLOYEES:
										query = "SELECT * FROM Employees;";
										break;
								}
								
								List<List<String>> rows = RunQueryNoBinding(query, db, dbStmt);
								const string json = "{{\r\n\t\"colNames\": [{0}],\r\n\t\"rows\": [{1}]\r\n}}\r\n";
								string jsonColNames = "";
								string jsonRows = "";
								
								if (rows.Count > 0) {
									var colNames = rows[0];
								
									foreach (string colName in colNames) {
										jsonColNames += "\"" + colName + "\",";
									}
									jsonColNames = jsonColNames.Substring(0, jsonColNames.Length - 1);
									
									string rowStr = "";
									for (int i = 1; i < rows.Count; ++i) {
										var row = rows[i];
										foreach(string val in row) {
											rowStr += "\"" + val + "\",";
										}
										rowStr = "[" + rowStr.Substring(0, rowStr.Length - 1) + "]";
										jsonRows += rowStr + ",\r\n\t\t";
										rowStr = "";
									}
									jsonRows = jsonRows.Substring(0, jsonRows.Length - 5);
								}
								
								body = String.Format(json, jsonColNames, jsonRows);
								
								headersTemplate = "Content-Type: application/json; charset=utf-8\r\n" +
									"Content-Length: {0}\r\n" +
									"Date: {1}";
									
								bodyUtf8Bytes = Encoding.Convert(utf16, utf8, utf16.GetBytes(body));
								dateTimeStr = System.DateTime.Now.ToString(dateTimePattern);
								headersStr = String.Format(headersTemplate,
													bodyUtf8Bytes.Length,
													dateTimeStr);
								responseStr = String.Format(responseTemplate,
												RESP_OK,
												headersStr,
												body);
								
								break;
							
							case RequestMsg.POST_ORDERS:
							case RequestMsg.POST_CUSTOMERS:
							case RequestMsg.POST_SUPPLIERS:
							case RequestMsg.POST_SHIPPERS:
							case RequestMsg.POST_PRODUCTS:
							case RequestMsg.POST_CATEGORIES:
							case RequestMsg.POST_EMPLOYEES:
								switch (requestMsg) {
									case RequestMsg.POST_CUSTOMERS:
										break;
								} // TODO: Pick up here.. RunQuery(binding)...
								break;
							default:
								throw new NotImplementedException();
						}
						
					} catch (Exception ex) {
						Console.Error.WriteLine(ex.Message);
						Console.Error.WriteLine(ex.StackTrace);
						dateTimeStr = System.DateTime.Now.ToString(dateTimePattern);
						responseStr = "HTTP/1.1 500 Internal Server Error\r\nDate: {0}\r\nContent-Length: 0\r\n\r\n";
						responseStr = String.Format(responseStr, dateTimeStr);
						closeConnection = true;
						
					} finally {
						Debug.Assert(responseStr.Length > 0, "No response string");
						//Byte[] utf16Bytes = utf16.GetBytes(RESP_OK);
						Byte[] utf8Bytes = Encoding.Convert(utf16, utf8, utf16.GetBytes(responseStr));
						
						Int32 numSent = ChildSocket.Send(utf8Bytes, 0, utf8Bytes.Length, SocketFlags.None,
							out socketErr);
						Console.WriteLine("{0}: Sent {1} bytes", Id, numSent);
						if (socketErr == SocketError.Success) {
							Debug.Assert(numSent == utf8Bytes.Length, "Sent bytes matches bytes length");
						} else {
							Console.Error.WriteLine("{0}: ChildSocket.Send error: {1}", Id, socketErr);
							stop = true;
						}
						
						if (closeConnection) stop = true;
						
					}
				}
			}
		
		} catch (Exception ex) {
			Console.Error.WriteLine("{0} Thread {1} error for request:\n{2}", System.DateTime.Now.ToString(), 
									Id, requestStr);
			Console.Error.WriteLine(ex.Message);
			Console.Error.WriteLine(ex.StackTrace);
		
		} finally {
			Console.WriteLine("{0} Thread {1} closing...", System.DateTime.Now.ToString(), Id);
			try {
				ChildSocket.Shutdown(SocketShutdown.Both);
			} catch (Exception ex) {
				Console.Error.WriteLine(ex.Message);
				Console.Error.WriteLine(ex.StackTrace);
			}
			
			if (db != 0) {
				SqliteCode errCode = sqlite3_finalize(dbStmt);
				if (errCode != SqliteCode.SQLITE_OK) {
					Console.Error.WriteLine("Thread {0} final sqlite3_finalize errCode: {1}", Id, errCode);
				}
			
				errCode = sqlite3_close(db);
				if (errCode != SqliteCode.SQLITE_OK) {
					Console.Error.WriteLine("Thread {0} sqlite3_close errCode: {1}", Id, errCode);
				}
			}

			ChildSocket.Close();				
		}
	}
}
