package xpnp;

import java.io.IOException;
import java.nio.ByteBuffer;

public class XpNamedPipe {
    private final static int ERROR_CODE_TIMEOUT = 1;

    private long namedPipeHandle = 0;
    
    static {
        try {
            System.loadLibrary("XpNamedPipeJni");
        } catch (Throwable t) {
            System.loadLibrary("XpNamedPipeJni_64");
        }
    }
    
    public static XpNamedPipe createNamedPipe(String shortName, boolean privatePipe) throws IOException {
        String pipeName = makePipeName(shortName, privatePipe);
        if (pipeName == null) {
            throw new IOException("Failed to make pipe name: " + getErrorMessage());
        }
        long pipeHandle = createPipe(pipeName, privatePipe);
        if (pipeHandle == 0) {
            throw new IOException("Failed to create named pipe: " + getErrorMessage());
        }
        return new XpNamedPipe(pipeHandle);
    }
    
    public static XpNamedPipe openNamedPipe(String shortName, boolean privatePipe) throws IOException {
        String pipeName = makePipeName(shortName, privatePipe);
        if (pipeName == null) {
            throw new IOException("Failed to make pipe name: " + getErrorMessage());
        }
        long pipeHandle = openPipe(pipeName, privatePipe);
        if (pipeHandle == 0) {
            throw new IOException("Failed to open named pipe: " + getErrorMessage());
        }
        return new XpNamedPipe(pipeHandle);
    }
    
    public void stop() throws IOException {
        if (!stopPipe(namedPipeHandle)) {
            throw new IOException("Failed to stop pipe: " + getErrorMessage());
        }
    }
    
    public synchronized void close() {
        if (namedPipeHandle != 0) {
            closePipe(namedPipeHandle);
            namedPipeHandle = 0;
        }
    }
    
    public XpNamedPipe acceptConnection() throws IOException {
        return acceptConnection(-1);
    }
    
    public XpNamedPipe acceptConnection(int timeoutMsecs) throws TimeoutException, IOException {
        long pipeHandle = acceptConnection(namedPipeHandle, timeoutMsecs);
        if (pipeHandle == 0) {
            if (getErrorCode() == ERROR_CODE_TIMEOUT) {
                throw new TimeoutException("Timeout waiting for client to connect: " + getErrorMessage());
            }
            throw new IOException("Failed to accept connection: " + getErrorMessage());
        }
        return new XpNamedPipe(pipeHandle);
    }
    
    public int read(byte[] buffer) throws TimeoutException, IOException  {
        return read(buffer, -1);
    }
    
    public int read(byte[] buffer, int timeoutMsecs) throws TimeoutException, IOException  {
        int bytesRead = readPipe(namedPipeHandle, buffer, timeoutMsecs);
        if (bytesRead == 0) {
            if (getErrorCode() == ERROR_CODE_TIMEOUT) {
                throw new TimeoutException("Timeout waiting for data: " + getErrorMessage());
            }
            throw new IOException("Failed to read pipe: " + getErrorMessage());
        }
        return bytesRead;
    }
   
    public void readBytes(byte[] buffer, int bytesToRead) throws TimeoutException, IOException  {
        readBytes(buffer, bytesToRead, -1);
    }
    
    public void readBytes(byte[] buffer, int bytesToRead, int timeoutMsecs) throws TimeoutException, IOException  {
        if (!readBytes(namedPipeHandle, buffer, bytesToRead, timeoutMsecs)) {
            if (getErrorCode() == ERROR_CODE_TIMEOUT) {
                throw new TimeoutException("Timeout waiting for data: " + getErrorMessage());
            }
            throw new IOException("Failed to read bytes: " + getErrorMessage());
        }
    }
    
    public byte[] readMessage() throws TimeoutException, IOException  {
        return readMessage(-1);
    }
    
    public byte[] readMessage(int timeoutMsecs) throws TimeoutException, IOException  {
        ByteBuffer lengthBuffer = ByteBuffer.allocate(4);
        readBytes(lengthBuffer.array(), 4, timeoutMsecs);
        int length = lengthBuffer.getInt();
        byte[] buffer = new byte[length];
        readBytes(buffer, length, timeoutMsecs);
        return buffer;
    }
    
    public void writeMessage(byte[] buffer) throws IOException {
        ByteBuffer lengthBuffer = ByteBuffer.allocate(4);
        lengthBuffer.putInt(buffer.length);
        write(lengthBuffer.array());
        write(buffer);
    }
    
    public void write(byte[] buffer) throws IOException {
        if (!writePipe(namedPipeHandle, buffer)) {
            throw new IOException("Failed to write to pipe: " + getErrorMessage());
        }
    }
    private XpNamedPipe(long pipeHandle) {
        this.namedPipeHandle = pipeHandle;
    }
    
    @Override 
    protected void finalize() {
        close();
    }
    
    private static native String makePipeName(String shortName, boolean userLocal);

    private static native String getErrorMessage();
    
    private static native int getErrorCode();
    
    private static native long createPipe(String fullName, boolean privatePipe);
    
    private static native boolean stopPipe(long pipeHandle);

    private static native boolean closePipe(long pipeHandle);
    
    private static native long acceptConnection(long pipeHandle, int timeoutMsecs);
    
    private static native int readPipe(long pipeHandle, byte[] buffer, int timeoutMsecs);
    
    private static native boolean readBytes(long pipeHandle, byte[] buffer, int bytesToRead, int timeoutMsecs);

    private static native long openPipe(String fullName, boolean privatePipe);

    private static native boolean writePipe(long pipeHandle, byte[] pipeMsg);
    
}
