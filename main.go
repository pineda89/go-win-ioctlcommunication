package main

import (
	"golang.org/x/sys/windows"
	"log"
	"syscall"
	"unsafe"
)

const (
	PROTOCOL_MSG_READ = 0
)

var (
	driverHandler uintptr
	targetPid     uint32

	kernel32 = syscall.MustLoadDLL("kernel32.dll")
	procWriteFile                          = kernel32.MustFindProc("WriteFile")
	getCurrentProcessId        = kernel32.MustFindProc("GetCurrentProcessId")
)



func main() {
	driverHandler = createHandle("\\\\.\\driverIOCTL")

	targetPid = 7880

	address := readAddress(0x7FFFF00F04F0, 8)
	log.Println("address", address)
}




func readAddress(address uintptr, size int) uintptr {
	var writeMe uintptr

	rStruct := readStruct{&writeMe, address, uintptr(size), uint32(GetCurrentProcessId()), targetPid, true, PROTOCOL_MSG_READ }

	WriteFile(driverHandler, uintptr(unsafe.Pointer(&rStruct)), unsafe.Sizeof(rStruct), nil, nil)

	return writeMe
}

func createHandle(name string) uintptr {
	pname, err := windows.UTF16PtrFromString(name)
	if err != nil {
		return 0
	}
	handle, err := windows.CreateFile(pname, windows.GENERIC_READ | windows.GENERIC_WRITE, windows.FILE_SHARE_READ | windows.FILE_SHARE_WRITE, nil, windows.OPEN_EXISTING, 0, 0)
	if err != nil {
		return 0
	}
	return uintptr(handle)
}

func WriteFile(handle uintptr, buf uintptr, bufLen uintptr, done *uint32, overlapped *uintptr) (err error) {
	_, _, e1 := syscall.Syscall6(procWriteFile.Addr(), 5, uintptr(handle), uintptr(unsafe.Pointer(buf)), bufLen, uintptr(unsafe.Pointer(done)), uintptr(unsafe.Pointer(overlapped)), 0)
	return e1
}

func GetCurrentProcessId() uintptr {
	id, _, _ := getCurrentProcessId.Call()
	return uintptr(id)
}

type readStruct struct {
	UserBufferAdress *uintptr
	GameAddressOffset uintptr
	ReadSize uintptr
	UserPID uint32
	GamePID uint32
	WriteOrRead bool
	ProtocolMsg uint32
}