package xpnp;

import xpnp.XpNamedPipe;

public class XpNamedPipeServer implements Runnable {

    /**
     * @param args
     */
    public static void main(String[] args) {
        XpNamedPipeServer server = new XpNamedPipeServer();
        server.run();
    }
    
    @Override
    public void run() {
        long listenPipeHandle = 0;
        long newPipeHandle = 0;
        try {
            String pipeName = XpNamedPipe.makePipeName("historyminer", true);
            if (pipeName == null) {
                System.out.println("makePipeName failed: " + XpNamedPipe.getErrorMessage());
                return;
            }
            System.out.println("pipeName = " + pipeName);
            
            listenPipeHandle = XpNamedPipe.createPipe(pipeName, true);
            if (listenPipeHandle == 0) {
                System.out.println("createPipe failed: " + XpNamedPipe.getErrorMessage());
                return;
            }
            
            newPipeHandle = XpNamedPipe.acceptConnection(listenPipeHandle);
            if (newPipeHandle == 0) {
                System.out.println("acceptConnection failed: " + XpNamedPipe.getErrorMessage());
                return;
            }
            
            System.out.println("connectPipe returned");
            
            byte [] msg = null;
            do {
                msg = XpNamedPipe.readPipe(newPipeHandle);
                if (msg != null) {
                    System.out.println("Message = " + new String(msg, "UTF-8"));
                }
            } while (msg != null);
            System.out.println("readPipe returned error: " + XpNamedPipe.getErrorMessage());
        } catch (Exception e) {
            System.out.println("Server caught exception: " + e);
        } finally {
            if (newPipeHandle != 0) {
                if (!XpNamedPipe.closePipe(newPipeHandle)) {
                    System.out.println("closePipe returned error: " + XpNamedPipe.getErrorMessage());
                }
            }
            if (listenPipeHandle != 0) {
                if (!XpNamedPipe.closePipe(listenPipeHandle)) {
                    System.out.println("closePipe returned error: " + XpNamedPipe.getErrorMessage());
                }
            }
        }
    }

}
