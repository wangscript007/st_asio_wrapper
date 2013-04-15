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

#include <boost/enable_shared_from_this.hpp>

#include "st_asio_wrapper_service_pump.h"
#include "st_asio_wrapper_socket.h"

namespace st_asio_wrapper
{

class i_server
{
public:
	virtual ~i_server() {}

	virtual st_service_pump& get_service_pump() = 0;
	virtual const st_service_pump& get_service_pump() const = 0;
	virtual void del_client(const boost::shared_ptr<st_socket>& client_ptr) = 0;
};

class st_server_socket : public st_socket, public boost::enable_shared_from_this<st_server_socket>
{
public:
	st_server_socket(i_server& server_) : st_socket(server_.get_service_pump()), server(server_) {}
	virtual void start() {do_recv_msg();}
	//when resue this st_server_socket, st_server_base will invoke reuse(), child must re-write this to init
	//all member variables, and then do not forget to invoke st_server_socket::reuse() to init father's
	//member variables
	virtual void reuse() {reset();}

protected:
	virtual void on_unpack_error() {unified_out::error_out("can not unpack msg."); force_close();}
	//do not forget to force_close this st_socket(in del_client(), there's a force_close() invocation)
	virtual void on_recv_error(const error_code& ec)
	{
#ifdef AUTO_CLEAR_CLOSED_SOCKET
		show_info("client:", "quit.");
		force_close();
		direct_dispatch_all_msg();
#else
		server.del_client(this->shared_from_this());
#endif
	}

protected:
	i_server& server;
};

} //namespace

#endif /* ST_ASIO_WRAPPER_SERVER_SOCKET_H_ */
