# Simple HTTP Server in C++

A lightweight, multithreaded HTTP server implementation in C++ using Windows Sockets (Winsock). This server supports basic HTTP operations and file serving capabilities.

## Features

- 🚀 **Multithreaded** - Handles multiple client connections simultaneously
- 📁 **File Operations** - Supports GET, POST, PUT, and DELETE operations
- 🔒 **Thread-Safe** - Uses mutex for file operation synchronization
- 🎯 **MIME Type Detection** - Automatically detects file types for proper content delivery
- 💻 **Windows Native** - Built using Windows Sockets API

## Supported HTTP Methods

- **GET** - Retrieve files from the server
- **HEAD** - Get file headers without content
- **POST** - Create new files
- **PUT** - Update existing files
- **DELETE** - Remove files

## Requirements

- Windows OS
- C++17 compatible compiler (GCC or MSVC)
- Winsock2 library

## Building

### Using GCC (MinGW)

```bash
g++ -std=c++17 http.cpp -o http.exe -lws2_32
```

### Using MSVC (Visual Studio)

1. Open Developer Command Prompt for VS
2. Navigate to project directory
3. Run:
```bash
cl /EHsc /std:c++17 http.cpp ws2_32.lib
```

## Usage

1. Create a `www` directory in the same folder as the executable
2. Run the server:
```bash
./http.exe
```
3. The server will start listening on `127.0.0.1:8080`

## Directory Structure

```
.
├── http.cpp          # Main server implementation
├── http.exe          # Compiled executable
└── www/              # Web root directory
    ├── index.html    # Default index file
    └── ...           # Other web files
```

## Example Requests

### GET Request
```http
GET /index.html HTTP/1.1
Host: localhost:8080
```

### POST Request
```http
POST /newfile.txt HTTP/1.1
Host: localhost:8080
Content-Length: 12

Hello World!
```

### PUT Request
```http
PUT /existing.txt HTTP/1.1
Host: localhost:8080
Content-Length: 15

Updated content!
```

### DELETE Request
```http
DELETE /file.txt HTTP/1.1
Host: localhost:8080
```

## Error Codes

- 200 OK - Successful request
- 201 Created - Resource created successfully
- 204 No Content - Successful deletion
- 400 Bad Request - Malformed request
- 403 Forbidden - Access denied
- 404 Not Found - Resource not found
- 405 Method Not Allowed - Unsupported HTTP method
- 500 Internal Server Error - Server-side error

## Security Considerations

⚠️ **Note**: This is a basic implementation and should not be used in production without additional security measures:

- No input validation
- No path sanitization
- No HTTPS support
- No authentication
- No rate limiting
- No logging

## Limitations

- Windows-only implementation
- No persistent connections
- No chunked transfer encoding
- No support for large file streaming
- Basic error handling
- No configuration options

## Contributing

Feel free to submit issues and enhancement requests!

## License

This project is open source and available under the [MIT License](LICENSE).

## Author

[Your Name]

## Acknowledgments

- Windows Sockets API
- C++ Standard Library 