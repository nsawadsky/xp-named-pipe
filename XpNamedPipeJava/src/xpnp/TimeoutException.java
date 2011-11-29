package xpnp;

import java.io.IOException;

public class TimeoutException extends IOException {

    public TimeoutException() {
    }

    public TimeoutException(String arg0) {
        super(arg0);
    }

    public TimeoutException(Throwable arg0) {
        super(arg0);
    }

    public TimeoutException(String arg0, Throwable arg1) {
        super(arg0, arg1);
    }

}
