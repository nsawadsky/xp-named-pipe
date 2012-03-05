package xpnp;

import java.io.IOException;

public class TestXpnpSecurity {

    public static void main(String[] args) {
        XpNamedPipe pipe = null;
        try {
            pipe = XpNamedPipe.openNamedPipe("S-1-5-21-3327237310-981301597-340182190-1006\\reverb-index", false);
            System.out.println("Opened pipe successfully");
        } catch (IOException e) {
            System.out.println("Error opening named pipe: " + e);
        } finally {
            if (pipe != null) {
                pipe.close();
            }
        }

    }

}
