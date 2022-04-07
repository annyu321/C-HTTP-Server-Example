#C++ HTTP Server Example

##Functionalities:
1. Expose GET service through HTTP
      Video, audio, image, text
2. Process connections, read requests and construct responses asynchronously
3. Support concurrent execution
4. Monitor the count of requests and session time
5. Exception Capture and error handling
6. Modern C++ feature 
      Use smart pointer to ensure that free of memory leaks and are exception-safe
      Use Lambda function as callbacks
7. Portable across the Windows and Ubuntu

#Dependencies
- Boost 1.77.0
- Visual Studio 2019
- HTTP 2.0

## Execution
- go run main.go aws.go handler.go

## Test
Use AWS CLI 
- http://localhost:8000/video
- http://localhost:8000/audio
- http://localhost:8000/image
- http://localhost:8000/text
