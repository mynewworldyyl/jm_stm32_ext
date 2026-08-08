import java.io.IOException;
import java.net.*;
import java.text.SimpleDateFormat;
import java.util.Date;

public class UdpServer {
    public static void main(String[] args) {
        int port = 9999;
        if (args.length > 0) {
            port = Integer.parseInt(args[0]);
        }
        DatagramSocket socket = null;
        try {
            socket = new DatagramSocket(port);
            System.out.println("UDP Server started on port " + port);
            byte[] buffer = new byte[1024];
            while (true) {
                DatagramPacket request = new DatagramPacket(buffer, buffer.length);
                socket.receive(request);
                String received = new String(request.getData(), 0, request.getLength(), "UTF-8").trim();
                String timestamp = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss").format(new Date());
                String responseText = "[" + timestamp + "] " + received;
                byte[] responseData = responseText.getBytes("UTF-8");
                DatagramPacket response = new DatagramPacket(responseData, responseData.length, request.getAddress(), request.getPort());
                socket.send(response);
                System.out.println("Received: " + received + " -> Responded: " + responseText);
            }
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            if (socket != null && !socket.isClosed()) {
                socket.close();
            }
        }
    }
}
