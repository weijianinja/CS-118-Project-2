/* 
 * Data structure for packet in our GBN implementation
 *  Type indicates what kind of packet it is
 *		0 - request
 *		1 - ACK
 *		2 - data
 *	If type == data, then seq_no denotes the current packet's sequence number
 *	Length is the total length of the file if type == data
 *	Data carries the request or actual data; empty if type == ACK
 */

int DATA_SIZE = 128;
struct packet
{
  int type;
  int seq_no;
  int length;
  char data[DATA_SIZE];
};