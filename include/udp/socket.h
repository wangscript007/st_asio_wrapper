/*
 * socket.h
 *
 *  Created on: 2012-3-2
 *      Author: youngwolf
 *		email: mail2tao@163.com
 *		QQ: 676218192
 *		Community on QQ: 198941541
 *
 * UDP socket
 */

#ifndef ST_ASIO_UDP_SOCKET_H_
#define ST_ASIO_UDP_SOCKET_H_

#include "../socket.h"

namespace st_asio_wrapper { namespace udp {

template <typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = boost::asio::ip::udp::socket, typename Family = boost::asio::ip::udp,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class generic_socket : public socket<Socket, Family, Packer, Unpacker, udp_msg<typename Packer::msg_type, Family>, udp_msg<typename Unpacker::msg_type, Family>, InQueue, InContainer, OutQueue, OutContainer>
{
public:
	typedef udp_msg<typename Packer::msg_type, Family> in_msg_type;
	typedef const in_msg_type in_msg_ctype;
	typedef udp_msg<typename Unpacker::msg_type, Family> out_msg_type;
	typedef const out_msg_type out_msg_ctype;

private:
	typedef socket<Socket, Family, Packer, Unpacker, in_msg_type, out_msg_type, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	static bool set_addr(boost::asio::ip::udp::endpoint& endpoint, unsigned short port, const std::string& ip)
	{
		if (ip.empty())
			endpoint = boost::asio::ip::udp::endpoint(ST_ASIO_UDP_DEFAULT_IP_VERSION, port);
		else
		{
			boost::system::error_code ec;
#if BOOST_ASIO_VERSION >= 101100
			BOOST_AUTO(addr, boost::asio::ip::make_address(ip, ec)); assert(!ec);
#else
			BOOST_AUTO(addr, boost::asio::ip::address::from_string(ip, ec)); assert(!ec);
#endif
			if (ec)
			{
				unified_out::error_out("invalid IP address %s.", ip.data());
				return false;
			}

			endpoint = boost::asio::ip::udp::endpoint(addr, port);
		}

		return true;
	}

protected:
	generic_socket(boost::asio::io_context& io_context_) : super(io_context_), has_bound(false), matrix(NULL) {}
	generic_socket(Matrix& matrix_) : super(matrix_.get_service_pump()), has_bound(false), matrix(&matrix_) {}

public:
	virtual bool is_ready() {return has_bound;}
	virtual void send_heartbeat()
	{
		in_msg_type msg(peer_addr);
		ST_THIS packer()->pack_heartbeat(msg);
		do_direct_send_msg(msg);
	}
	virtual const char* type_name() const {return "UDP";}
	virtual int type_id() const {return 0;}

	//reset all, be ensure that there's no any operations performed on this socket when invoke it
	//subclass must re-write this function to initialize itself, and then do not forget to invoke superclass' reset function too
	//notice, when reusing this socket, object_pool will invoke this function, so if you want to do some additional initialization
	// for this socket, do it at here and in the constructor.
	//for udp::single_service_base, this virtual function will never be called, please note.
	virtual void reset()
	{
		has_bound = false;

		sending_msg.clear();
		super::reset();
	}

	bool set_local_addr(unsigned short port, const std::string& ip = std::string()) {return set_addr(local_addr, port, ip);}
	bool set_local_addr(const std::string& file_name) {local_addr = typename Family::endpoint(file_name); return true;}
	const typename Family::endpoint& get_local_addr() const {return local_addr;}

	bool set_peer_addr(unsigned short port, const std::string& ip = std::string()) {return set_addr(peer_addr, port, ip);}
	bool set_peer_addr(const std::string& file_name) {peer_addr = typename Family::endpoint(file_name); return true;}
	const typename Family::endpoint& get_peer_addr() const {return peer_addr;}

	void disconnect() {force_shutdown();}
	void force_shutdown() {show_info("link:", "been shutting down."); ST_THIS dispatch_strand(rw_strand, boost::bind(&generic_socket::close_, this));}
	void graceful_shutdown() {force_shutdown();}

	std::string endpoint_to_string(const boost::asio::ip::udp::endpoint& ep) const {return ep.address().to_string() + ':' + boost::to_string(ep.port());}
#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
	std::string endpoint_to_string(const boost::asio::local::datagram_protocol::endpoint& ep) const {return ep.path();}
#endif

	void show_info(const char* head = NULL, const char* tail = NULL) const
	{
		unified_out::info_out(ST_ASIO_LLF " %s %s %s",
			ST_THIS id(), NULL == head ? "" : head, endpoint_to_string(local_addr).data(), NULL == tail ? "" : tail);
	}

	void show_status() const
	{
		std::stringstream s;

		if (stat.last_send_time > 0)
		{
			s << "\n\tlast send time: ";
			log_formater::to_time_str(stat.last_send_time, s);
		}

		if (stat.last_recv_time > 0)
		{
			s << "\n\tlast recv time: ";
			log_formater::to_time_str(stat.last_recv_time, s);
		}

		unified_out::info_out(
			"\n\tid: " ST_ASIO_LLF
			"\n\tstarted: %d"
			"\n\tsending: %d"
#ifdef ST_ASIO_PASSIVE_RECV
			"\n\treading: %d"
#endif
			"\n\tdispatching: %d"
			"\n\trecv suspended: %d"
			"\n\tsend buffer usage: %.2f%%"
			"\n\trecv buffer usage: %.2f%%"
			"%s",
			ST_THIS id(), ST_THIS started(), ST_THIS is_sending(),
#ifdef ST_ASIO_PASSIVE_RECV
			ST_THIS is_reading(),
#endif
			ST_THIS is_dispatching(), ST_THIS is_recv_idle(),
			ST_THIS send_buf_usage() * 100.f, ST_THIS recv_buf_usage() * 100.f, s.str().data());
	}

	///////////////////////////////////////////////////
	//msg sending interface
	//if the message already packed, do call direct_send_msg or direct_sync_send_msg to reduce unnecessary memory replication, if you will not
	// use it any more, call the one that accepts reference of a message.
	UDP_SEND_MSG(send_msg, false) //use the packer with native = false to pack the msgs
	UDP_SEND_MSG(send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into udp::generic_socket's send buffer
	UDP_SAFE_SEND_MSG(safe_send_msg, send_msg)
	UDP_SAFE_SEND_MSG(safe_send_native_msg, send_native_msg)

#ifdef ST_ASIO_SYNC_SEND
	UDP_SYNC_SEND_MSG(sync_send_msg, false) //use the packer with native = false to pack the msgs
	UDP_SYNC_SEND_MSG(sync_send_native_msg, true) //use the packer with native = true to pack the msgs
	//guarantee send msg successfully even if can_overflow equal to false
	//success at here just means put the msg into udp::generic_socket's send buffer
	UDP_SYNC_SAFE_SEND_MSG(sync_safe_send_msg, sync_send_msg)
	UDP_SYNC_SAFE_SEND_MSG(sync_safe_send_native_msg, sync_send_native_msg)
#endif
	//msg sending interface
	///////////////////////////////////////////////////

protected:
	Matrix* get_matrix() {return matrix;}
	const Matrix* get_matrix() const {return matrix;}

	virtual bool bind(const typename Family::endpoint& local_addr) {return true;}

	virtual bool do_start()
	{
		BOOST_AUTO(&lowest_object, ST_THIS lowest_layer());
		if (!lowest_object.is_open()) //user maybe has opened this socket (to set options for example)
		{
			boost::system::error_code ec;
			lowest_object.open(local_addr.protocol(), ec); assert(!ec);
			if (ec)
			{
				unified_out::error_out("cannot create socket: %s", ec.message().data());
				return (has_bound = false);
			}

#ifndef ST_ASIO_NOT_REUSE_ADDRESS
			lowest_object.set_option(boost::asio::socket_base::reuse_address(true), ec); assert(!ec);
#endif
		}

		if (!bind(local_addr))
			return (has_bound = false);

		return (has_bound = true) && super::do_start();
	}

	//msg was failed to send and udp::generic_socket will not hold it any more, if you want to re-send it in the future,
	// you must take over it and re-send (at any time) it via direct_send_msg.
	//DO NOT hold msg for future using, just swap its content with your own message in this virtual function.
	virtual void on_send_error(const boost::system::error_code& ec, typename super::in_msg& msg)
		{unified_out::error_out(ST_ASIO_LLF " send msg error (%d %s)", ST_THIS id(), ec.value(), ec.message().data());}

	virtual void on_recv_error(const boost::system::error_code& ec)
	{
		if (boost::asio::error::operation_aborted != ec)
			unified_out::error_out(ST_ASIO_LLF " recv msg error (%d %s)", ST_THIS id(), ec.value(), ec.message().data());
#ifndef ST_ASIO_CLEAR_OBJECT_INTERVAL
		else if (NULL != matrix)
			matrix->del_socket(ST_THIS id());
#endif
	}

	virtual bool on_heartbeat_error()
	{
		stat.last_recv_time = time(NULL); //avoid repetitive warnings
		unified_out::warning_out(ST_ASIO_LLF " %s is not available", ST_THIS id(), endpoint_to_string(peer_addr).data());
		return true;
	}

#ifdef ST_ASIO_SYNC_SEND
	virtual void on_close() {if (sending_msg.p) sending_msg.p->set_value(NOT_APPLICABLE); super::on_close();}
#endif

private:
	using super::close;
	using super::handle_error;
	using super::handle_msg;
	using super::do_direct_send_msg;
#ifdef ST_ASIO_SYNC_SEND
	using super::do_direct_sync_send_msg;
#endif

	void close_() {close(true);} //workaround for old compilers, otherwise, we can bind to close directly in dispatch_strand

	virtual void do_recv_msg()
	{
#ifdef ST_ASIO_PASSIVE_RECV
		if (reading)
			return;
#endif
		BOOST_AUTO(recv_buff, ST_THIS unpacker()->prepare_next_recv());
		assert(boost::asio::buffer_size(recv_buff) > 0);
		if (0 == boost::asio::buffer_size(recv_buff))
			unified_out::error_out(ST_ASIO_LLF " the unpacker returned an empty buffer, quit receiving!", ST_THIS id());
		else
		{
#ifdef ST_ASIO_PASSIVE_RECV
			reading = true;
#endif
			ST_THIS next_layer().async_receive_from(recv_buff, temp_addr, make_strand_handler(rw_strand,
				ST_THIS make_handler_error_size(boost::bind(&generic_socket::recv_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))));
		}
	}

	void recv_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec && bytes_transferred > 0)
		{
			stat.last_recv_time = time(NULL);

			typename Unpacker::container_type msg_can;
			ST_THIS unpacker()->parse_msg(bytes_transferred, msg_can);

#ifdef ST_ASIO_PASSIVE_RECV
			reading = false; //clear reading flag before call handle_msg() to make sure that recv_msg() can be called successfully in on_msg_handle()
#endif
			for (BOOST_AUTO(iter, msg_can.begin()); iter != msg_can.end(); ++iter)
				temp_msg_can.emplace_back(temp_addr, boost::ref(*iter));
			if (handle_msg()) //if macro ST_ASIO_PASSIVE_RECV been defined, handle_msg will always return false
				do_recv_msg(); //receive msg in sequence
		}
		else
		{
#ifdef ST_ASIO_PASSIVE_RECV
			reading = false; //clear reading flag before call handle_msg() to make sure that recv_msg() can be called successfully in on_msg_handle()
#endif
#if defined(_MSC_VER) || defined(__CYGWIN__) || defined(__MINGW32__) || defined(__MINGW64__)
			if (ec && boost::asio::error::connection_refused != ec && boost::asio::error::connection_reset != ec)
#else
			if (ec)
#endif
			{
				handle_error();
				on_recv_error(ec);
			}
			else if (handle_msg()) //if macro ST_ASIO_PASSIVE_RECV been defined, handle_msg will always return false
				do_recv_msg(); //receive msg in sequence
		}
	}

	virtual bool do_send_msg(bool in_strand = false)
	{
		if (!in_strand && sending)
			return true;

		if ((sending = send_buffer.try_dequeue(sending_msg)))
		{
			stat.send_delay_sum += statistic::now() - sending_msg.begin_time;

			sending_msg.restart();
			ST_THIS next_layer().async_send_to(boost::asio::buffer(sending_msg.data(), sending_msg.size()), sending_msg.peer_addr, make_strand_handler(rw_strand,
				ST_THIS make_handler_error_size(boost::bind(&generic_socket::send_handler, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred))));
			return true;
		}

		return false;
	}

	void send_handler(const boost::system::error_code& ec, size_t bytes_transferred)
	{
		if (!ec)
		{
			stat.last_send_time = time(NULL);

			stat.send_byte_sum += bytes_transferred;
			stat.send_time_sum += statistic::now() - sending_msg.begin_time;
			++stat.send_msg_sum;
#ifdef ST_ASIO_SYNC_SEND
			if (sending_msg.p)
				sending_msg.p->set_value(SUCCESS);
#endif
#ifdef ST_ASIO_WANT_MSG_SEND_NOTIFY
			ST_THIS on_msg_send(sending_msg);
#endif
#ifdef ST_ASIO_WANT_ALL_MSG_SEND_NOTIFY
			if (send_buffer.empty())
				ST_THIS on_all_msg_send(sending_msg);
#endif
		}
		else
		{
#ifdef ST_ASIO_SYNC_SEND
			if (sending_msg.p)
				sending_msg.p->set_value(NOT_APPLICABLE);
#endif
			on_send_error(ec, sending_msg);
		}
		sending_msg.clear(); //clear sending message after on_send_error, then user can decide how to deal with it in on_send_error

		if (ec && (boost::asio::error::not_socket == ec || boost::asio::error::bad_descriptor == ec))
			return;

		//send msg in sequence
		//on windows, sending a msg to addr_any may cause errors, please note
		//for UDP, sending error will not stop subsequent sending.
		if (!do_send_msg(true) && !send_buffer.empty())
			do_send_msg(true); //just make sure no pending msgs
	}

private:
	using super::stat;
	using super::temp_msg_can;

	using super::send_buffer;
	using super::sending;

#ifdef ST_ASIO_PASSIVE_RECV
	using super::reading;
#endif
	using super::rw_strand;

	bool has_bound;
	typename super::in_msg sending_msg;
	typename Family::endpoint local_addr;
	typename Family::endpoint temp_addr; //used when receiving messages
	typename Family::endpoint peer_addr;

	Matrix* matrix;
};

template <typename Packer, typename Unpacker, typename Matrix = i_matrix, typename Socket = boost::asio::ip::udp::socket,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class socket_base : public generic_socket<Packer, Unpacker, Matrix, Socket, boost::asio::ip::udp, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef generic_socket<Packer, Unpacker, Matrix, Socket, boost::asio::ip::udp, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	socket_base(boost::asio::io_context& io_context_) : super(io_context_) {}
	socket_base(Matrix& matrix_) : super(matrix_) {}

protected:
	virtual bool bind(const boost::asio::ip::udp::endpoint& local_addr)
	{
		if (0 != local_addr.port() || !local_addr.address().is_unspecified())
		{
			boost::system::error_code ec;
			ST_THIS lowest_layer().bind(local_addr, ec);
			if (ec && boost::asio::error::invalid_argument != ec)
			{
				unified_out::error_out("cannot bind socket: %s", ec.message().data());
				return false;
			}
		}

		return true;
	}
};

#ifdef BOOST_ASIO_HAS_LOCAL_SOCKETS
template <typename Packer, typename Unpacker, typename Matrix = i_matrix,
	template<typename> class InQueue = ST_ASIO_INPUT_QUEUE, template<typename> class InContainer = ST_ASIO_INPUT_CONTAINER,
	template<typename> class OutQueue = ST_ASIO_OUTPUT_QUEUE, template<typename> class OutContainer = ST_ASIO_OUTPUT_CONTAINER>
class unix_socket_base : public generic_socket<Packer, Unpacker, Matrix, boost::asio::local::datagram_protocol::socket, boost::asio::local::datagram_protocol, InQueue, InContainer, OutQueue, OutContainer>
{
private:
	typedef generic_socket<Packer, Unpacker, Matrix, boost::asio::local::datagram_protocol::socket, boost::asio::local::datagram_protocol, InQueue, InContainer, OutQueue, OutContainer> super;

public:
	unix_socket_base(boost::asio::io_context& io_context_) : super(io_context_) {}
	unix_socket_base(Matrix& matrix_) : super(matrix_) {}

protected:
	virtual bool bind(const boost::asio::local::datagram_protocol::endpoint& local_addr)
	{
		if (!local_addr.path().empty())
		{
			boost::system::error_code ec;
			ST_THIS lowest_layer().bind(local_addr, ec);
			if (ec && boost::asio::error::invalid_argument != ec)
			{
				unified_out::error_out("cannot bind socket: %s", ec.message().data());
				return false;
			}
		}

		return true;
	}
};
#endif

}} //namespace

#endif /* ST_ASIO_UDP_SOCKET_H_ */
