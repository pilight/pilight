import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.ByteArrayInputStream;
import java.io.StringReader;
import java.nio.charset.Charset;
import java.io.PrintStream;
import java.util.regex.Pattern;
import java.util.regex.Matcher;
import java.util.Collections;
import java.util.Enumeration;
import sun.net.*;
import java.net.*;
import java.util.Date;

public class process {
	public static void main(String[] args) {
		String line = null;
		DatagramSocket ssdp = null;
		PrintStream printStream = null;
		BufferedReader bufferedReader = null;
		int port = 0;
		String server = null;
		Socket socket = null;

		String msg = "M-SEARCH * HTTP/1.1\r\n"
					 + "Host:239.255.255.250:1900\r\n"
				     + "ST:urn:schemas-upnp-org:service:pilight:1\r\n"
					 + "Man:\"ssdp:discover\"\r\n"
					 + "MX:3\r\n\r\n";		

		try {
			Enumeration<NetworkInterface> nets = NetworkInterface.getNetworkInterfaces();
	        for(NetworkInterface netint : Collections.list(nets)) {
	        	Enumeration<InetAddress> inetAddresses = netint.getInetAddresses();
	        	for(InetAddress inetAddress : Collections.list(inetAddresses)) {
	        		if(!inetAddress.isLoopbackAddress() && inetAddress instanceof Inet4Address) {   
						try {
		        			ssdp = new DatagramSocket(new InetSocketAddress(inetAddress.getHostAddress().toString(), 0));
							byte[] buff = msg.getBytes();
							DatagramPacket sendPack = new DatagramPacket(buff, buff.length);
							sendPack.setAddress(InetAddress.getByName("239.255.255.250"));
							sendPack.setPort(1900);
				
							try {
								ssdp.send(sendPack);
								ssdp.setSoTimeout(10);
								boolean loop = true;
								while(loop) {
									DatagramPacket recvPack = new DatagramPacket(new byte[1024], 1024);
								  	ssdp.receive(recvPack);
								  	byte[] recvData = recvPack.getData();
								  	InputStreamReader recvInput = new InputStreamReader(new ByteArrayInputStream(recvData), Charset.forName("UTF-8"));
									StringBuilder recvOutput = new StringBuilder();
									for(int value; (value = recvInput.read()) != -1;) {
										recvOutput.append((char)value);
									}
									BufferedReader bufReader = new BufferedReader(new StringReader(recvOutput.toString()));
									Pattern pattern = Pattern.compile("Location:([0-9.]+):(.*)");
									while((line=bufReader.readLine()) != null) {
										Matcher matcher = pattern.matcher(line);
										if(matcher.matches()) {
											server = matcher.group(1);
											port = Integer.parseInt(matcher.group(2));
											loop = false;
											break;
										}
									}
								}
							} catch(SocketTimeoutException e) {
							} catch(IOException e) {
								System.out.println("no pilight ssdp connections found");
								ssdp.close();
								System.exit(0);
							}
						} catch(UnknownHostException e) {
						}
					}
	        	}
	        }
        } catch(SocketException e) {
		}
	
		if(server == null || port == 0) {
			System.out.println("no pilight ssdp connections found");
			System.exit(0);
		}

		try {
		   socket = new Socket();
		   socket.connect(new InetSocketAddress(server, port), 1000);
		
			try {
				if(printStream == null) {
					printStream = new PrintStream(socket.getOutputStream(), false);
				}
				printStream.print("{\"message\":\"client receiver\"}\n");
				printStream.flush();
			} catch(IOException e) {
				System.out.println("failed to write messages to server");
			}
			
			int size = 0;
			if(size == 0) {
				size = 1025;
			}
			try {
				if(bufferedReader == null) {
					bufferedReader = new BufferedReader(new InputStreamReader(socket.getInputStream(), "UTF-8"), size);
				}
				if(bufferedReader.ready()) {
					while((line = bufferedReader.readLine()) != null) {
						System.out.println(line);
					}
				}
			} catch(IOException e) {
				System.out.println("failed to receive messages from server");
			}

		} catch (UnknownHostException e) {
		   System.out.println("don't know about host");
		} catch(IOException e) {
		   System.out.println("could not get I/O for connection");
		}
   }
}
