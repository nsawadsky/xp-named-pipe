package xpnp;

import java.io.IOException;

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
            System.out.println("Created listening pipe, listening for 1 second");
            
            while (true) {
                XpNamedPipe newConn = null;
                try {
                    newConn = listeningPipe.acceptConnection(1000);
                } catch (IOException e) {
                    System.out.println("Caught IOException: " + e);
                }
                
                System.out.println("Listening indefinitely");
                newConn = listeningPipe.acceptConnection();
                
                System.out.println("Accepted connection");
                
                System.out.println("Waiting for 1 second for message");
                try {
                    newConn.readMessage(1000);
                } catch (IOException e) {
                    System.out.println("Caught IOException: " + e);
                }
                System.out.println("Waiting indefinitely for message");
                byte[] buffer = newConn.readMessage();
                
                System.out.println("Got message: " + new String(buffer, "UTF-8"));
                
                System.out.println("Sending response");
                newConn.writeMessage("Well, hi there!".getBytes("UTF-8"));
                
                Thread.sleep(2000);
                
                System.out.println("Closing pipe");
                
                newConn.close();
            }
        } catch (Exception e) {
            System.out.println("Server caught exception: " + e);
        }
    }

}
