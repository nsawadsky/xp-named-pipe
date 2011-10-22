package xpnp;

public class XpNamedPipe {
    static {
        try {
            System.loadLibrary("XpNamedPipeJni");
        } catch (Throwable t) {
            System.loadLibrary("XpNamedPipeJni_64");
        }
    }
   
    public static native String getErrorMessage();
    
    public static native String makePipeName(String shortName, boolean userLocal);
    
    public static native long createPipe(String fullName, boolean privatePipe);
    
    public static native long openPipe(String fullName);

    public static native boolean closePipe(long pipeHandle);
    
    public static native long acceptConnection(long pipeHandle);
    
    public static native byte[] readPipe(long pipeHandle);
    
    public static native boolean writePipe(long pipeHandle, byte[] pipeMsg);
    
}
