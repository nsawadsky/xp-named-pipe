package xpnp;

import java.io.IOException;

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
            final XpNamedPipe pipe = XpNamedPipe.openNamedPipe("historyminer", true);
            System.out.println("Opened pipe");
            
            new Thread(new Runnable() {

                @Override
                public void run() {
                    try {
                        System.out.println("Reading thread started");
                        while (true) {
                            byte[] msg = pipe.readMessage();
                            System.out.println("Reading thread got message: " + new String(msg, "UTF-8"));
                        }
                    } catch (IOException e) {
                        System.out.println("Reading thread caught IOException: " + e);
                    }
                    
                }
                
            }).start();
            
            Thread.sleep(2000);
            
            pipe.writeMessage("Hello, world!".getBytes("UTF-8"));
            System.out.println("Wrote message");
            
            Thread.sleep(1000);
            
            System.out.println("Stopping reading thread");
            pipe.stop();
            
        } catch (Exception e) {
            System.out.println("Client caught exception: " + e);
        } 
    }

}
