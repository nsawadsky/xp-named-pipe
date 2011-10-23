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
            XpNamedPipe pipe = XpNamedPipe.openNamedPipe("historyminer", true);
            System.out.println("Opened pipe");
            
            pipe.write(new String("Hello, world!").getBytes("UTF-8"));
            System.out.println("Wrote message");
            
            pipe.close();
        } catch (Exception e) {
            System.out.println("Client caught exception: " + e);
        } 
    }

}
