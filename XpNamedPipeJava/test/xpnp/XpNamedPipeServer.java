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
        try {
            XpNamedPipe listeningPipe = XpNamedPipe.createNamedPipe("historyminer", true);
            System.out.println("Created listening pipe");
            
            XpNamedPipe newConn = listeningPipe.acceptConnection();
            System.out.println("Accepted connection");
            
            byte[] buffer = new byte[1024];
            newConn.read(buffer);
            
            System.out.println("Got message: " + new String(buffer, "UTF-8"));
            
            newConn.close();
            listeningPipe.close();
        } catch (Exception e) {
            System.out.println("Server caught exception: " + e);
        }
    }

}
