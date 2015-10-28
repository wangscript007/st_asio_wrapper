/*
 * st_asio_wrapper_server_socket.h
 *
 *  Created on: 2013-4-11
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * this class only used at server endpoint
 */

#ifndef ST_ASIO_WRAPPER_SERVER_SOCKET_H_
#define ST_ASIO_WRAPPER_SERVER_SOCKET_H_

#include "st_asio_wrapper_service_pump.h"
#include "st_asio_wrapper_tcp_socket.h"

namespace st_asio_wrapper
{

class i_server
{
public:
	virtual st_service_pump& get_service_pump() = 0;
	virtual const st_service_pump& get_service_pump() const = 0;
	virtual void del_client(const boost::shared_ptr<st_timer>& client_ptr) = 0;
};

template<typename Packer = DEFAULT_PACKER, typename Unpacker = DEFAULT_UNPACKER, typename Server = i_server, typename Socket = boost::asio::ip::tcp::socket>
class st_server_socket_base : public st_tcp_socket_base<Socket, Packer, Unpacker>, public boost::enable_shared_from_this<st_server_socket_base<Packer, Unpacker, Server, Socket> >
{
public:
	st_server_socket_base(Server& server_) : st_tcp_socket_base<Socket, Packer, Unpacker>(server_.get_service_pump()), server(server_) {}

	template<typename Arg>
	st_server_socket_base(Server& server_, Arg& arg) : st_tcp_socket_base<Socket, Packer, Unpacker>(server_.get_service_pump(), arg), server(server_) {}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//please note, when reuse this socket, st_object_pool will invoke reset(), child must re-write it to initialize all member variables,
	//and then do not forget to invoke st_server_socket_base::reset() to initialize father's member variables
	virtual void reset() {st_tcp_socket_base<Socket, Packer, Unpacker>::reset();}

	void disconnect() {force_close();}
	void force_close()
	{
		if (!ST_THIS is_closing())
		{
			show_info("link:", "been closed.");
			ST_THIS close_state = 1;
		}
		else if (1 == ST_THIS close_state)
			return;

		st_tcp_socket_base<Socket, Packer, Unpacker>::force_close();
	}

	void graceful_close()
	{
		if (ST_THIS is_closing())
			return;

		show_info("link:", "been closing gracefully.");
		ST_THIS close_state = 2;
		st_tcp_socket_base<Socket, Packer, Unpacker>::graceful_close();
	}

	void show_info(const char* head, const char* tail) const
	{
		boost::system::error_code ec;
		BOOST_AUTO(ep, ST_THIS lowest_layer().remote_endpoint(ec));
		if (!ec)
			unified_out::info_out("%s %s:%hu %s", head, ep.address().to_string().c_str(), ep.port(), tail);
	}

	void show_info(const char* head, const char* tail, const boost::system::error_code& ec) const
	{
		boost::system::error_code ec2;
		BOOST_AUTO(ep, ST_THIS lowest_layer().remote_endpoint(ec2));
		if (!ec2)
			unified_out::info_out("%s %s:%hu %s (%d %s)", head, ep.address().to_string().c_str(), ep.port(), tail, ec.value(), ec.message().data());
	}

protected:
	virtual bool do_start()
	{
		if (!ST_THIS get_io_service().stopped())
		{
			ST_THIS do_recv_msg();
			return true;
		}

		return false;
	}

	virtual void on_unpack_error() {unified_out::error_out("can not unpack msg."); ST_THIS force_close();}
	//do not forget to force_close this socket(in del_client(), there's a force_close() invocation)
	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		ST_THIS show_info("link:", "broken/closed", ec);

#ifdef AUTO_CLEAR_CLOSED_SOCKET
		ST_THIS force_close();
#else
		server.del_client(boost::dynamic_pointer_cast<st_timer>(ST_THIS shared_from_this()));
#endif

		ST_THIS close_state = 0;
	}

protected:
	Server& server;
};
typedef st_server_socket_base<> st_server_socket;

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVER_SOCKET_H_ */
