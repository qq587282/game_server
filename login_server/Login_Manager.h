/*
 *  Created on: Dec 16, 2015
 *      Author: zhangyalei
 */


#ifndef LOGIN_MANAGER_H_
#define LOGIN_MANAGER_H_

#include "Common_Func.h"
#include "Public_Struct.h"
#include "Public_Define.h"
#include "Login_Timer.h"
#include "Block_Buffer.h"
#include "Thread.h"
#include "List.h"
#include "Block_List.h"
#include "Object_Pool.h"
#include "Check_Account.h"

class Login_Manager: public Thread {
public:
	typedef Object_Pool<Block_Buffer, Thread_Mutex> Block_Pool;
	typedef Block_List<Thread_Mutex> Data_List;
	typedef std::list<Close_Info> Close_List;
	typedef boost::unordered_map<int, int> Msg_Count_Map;
	typedef std::vector<Ip_Info> Ip_Vec;
	typedef boost::unordered_map<std::string, std::string> Account_Session_Map;

public:
	enum {
		STATUS_NORMAL = 1,
		STATUS_CLOSING = 2,
	};

	static Login_Manager *instance(void);

	int init(void);
	void run_handler(void);

	int init_gate_ip(void);
	void get_gate_ip(std::string &account, std::string &ip, int &port);

	int bind_account_session(std::string& account, std::string& session);
	int find_account_session(std::string& account, std::string& session);
	int unbind_account_session(std::string& account);

	/// 定时器处理
	int register_timer(void);
	int unregister_timer(void);

	/// 发送数据接口
	int send_to_client(int cid, Block_Buffer &buf);
	int send_to_gate(int cid, Block_Buffer &buf);

	/// 关闭连接
	int close_client(int cid);
	/// 服务器状态
	int server_status(void);
	/// 主动关闭处理
	int self_close_process(void);

	/// 通信层投递消息到Login_Manager
	int push_login_client_data(Block_Buffer *buf);
	int push_login_gate_data(Block_Buffer *buf);
	int push_self_loop_message(Block_Buffer &msg_buf);

	/// 消息处理
	int process_list();

	int tick(void);
	int close_list_tick(Time_Value &now);
	int server_info_tick(Time_Value &now);
	int manager_tick(Time_Value &now);
	void object_pool_tick(Time_Value &now);

	void get_server_info(Block_Buffer &buf);

	/// 返回上次tick的绝对时间, 最大误差有100毫秒
	/// 主要为减少系统调用gettimeofday()调用次数
	const Time_Value &tick_time(void);
	void object_pool_size(void);
	void free_cache(void);

	/// 统计内部消息量
	void set_msg_count_onoff(int v);
	void print_msg_count(void);
	void inner_msg_count(Block_Buffer &buf);
	void inner_msg_count(int msg_id);

	Check_Account &check_account(void);

private:
	Login_Manager(void);
	virtual ~Login_Manager(void);
	Login_Manager(const Login_Manager &);
	const Login_Manager &operator=(const Login_Manager &);

private:
	static Login_Manager *instance_;

	Block_Pool block_pool_;

	Data_List login_client_data_list_;				///client-->login
	Data_List login_gate_data_list_;		///login-->connector
	Data_List self_loop_block_list_; 				/// self_loop_block_list
	Close_List close_list_; /// 其中的连接cid在n秒后投递到通信层关闭

	Server_Info login_client_server_info_;
	Server_Info login_gate_server_info_;

	Tick_Info tick_info_;

	int status_;
	bool is_register_timer_;
	Time_Value tick_time_;

	/// 消息统计
	bool msg_count_onoff_;
	Msg_Count_Map inner_msg_count_map_;

	Ip_Vec gate_ip_vec_;
	Account_Session_Map accout_session_map_;

	Check_Account check_account_;
};

#define LOGIN_MANAGER Login_Manager::instance()

////////////////////////////////////////////////////////////////////////////////

inline int Login_Manager::push_login_client_data(Block_Buffer *buf) {
	login_client_data_list_.push_back(buf);
	return 0;
}

inline int Login_Manager::push_login_gate_data(Block_Buffer *buf) {
	login_gate_data_list_.push_back(buf);
	return 0;
}

inline int Login_Manager::push_self_loop_message(Block_Buffer &msg_buf) {
	Block_Buffer *buf = block_pool_.pop();
	if (! buf) {
		MSG_USER("block_pool_.pop return 0");
		return -1;
	}
	buf->reset();
	buf->copy(&msg_buf);
	self_loop_block_list_.push_back(buf);
	return 0;
}

inline const Time_Value &Login_Manager::tick_time(void) {
	return tick_time_;
}

inline void Login_Manager::set_msg_count_onoff(int v) {
	if (v == 0 || v == 1) {
		msg_count_onoff_ = v;
	} else {
		MSG_USER("error value v = %d", v);
	}
}

inline void Login_Manager::inner_msg_count(Block_Buffer &buf) {
	/// len(uint16), msg_id(uint32), status(uint32)
	if (msg_count_onoff_ && buf.readable_bytes() >= (sizeof(uint16_t) + sizeof(uint32_t) + sizeof(uint32_t))) {
		uint16_t read_idx_orig = buf.get_read_idx();

		buf.set_read_idx(read_idx_orig + sizeof(uint16_t));
		uint32_t msg_id = 0;
		buf.read_uint32(msg_id);
		++(inner_msg_count_map_[static_cast<int>(msg_id)]);

		buf.set_read_idx(read_idx_orig);
	}
}

inline void Login_Manager::inner_msg_count(int msg_id) {
	if (msg_count_onoff_) {
		++(inner_msg_count_map_[static_cast<int>(msg_id)]);
	}
}

inline Check_Account &Login_Manager::check_account(void) {
	return check_account_;
}


#endif /* LOGIN_MANAGER_H_ */