package xpnp;

import xpnp.XpNamedPipe;

public class XpNamedPipeClient implements Runnable {

    /**
     * @param args
     */
    public static void main(String[] args) {
        XpNamedPipeClient client = new XpNamedPipeClient();
        client.run();

    }

    @Override
    public void run() {
        try {
            String pipeName = XpNamedPipe.makePipeName("historyminer", true);
            if (pipeName == null) {
                System.out.println("makePipeName failed: " + XpNamedPipe.getErrorMessage());
                return;
            }
            System.out.println("pipeName = " + pipeName);
            long pipeHandle = XpNamedPipe.openPipe(pipeName);
            if (pipeHandle == 0) {
                System.out.println("openPipe failed: " + XpNamedPipe.getErrorMessage());
                return;
            }
            System.out.println("Opened pipe");
            
            if (XpNamedPipe.writePipe(pipeHandle, new String("Hello, world!").getBytes("UTF-8"))) {
                System.out.println("Wrote message");
            } else {
                System.out.println("Error writing to pipe: " + XpNamedPipe.getErrorMessage());
            }
            if (!XpNamedPipe.closePipe(pipeHandle)) {
                System.out.println("closePipe failed: " + XpNamedPipe.getErrorMessage());
            }

        } catch (Exception e) {
            System.out.println("Client caught exception: " + e);
        } 
    }

}
