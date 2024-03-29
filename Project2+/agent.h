/**
 * Framework for Threes! and its variants (C++ 11)
 * agent.h: Define the behavior of variants of agents including players and environments
 *
 * Author: Theory of Computer Games
 *         Computer Games and Intelligence (CGI) Lab, NYCU, Taiwan
 *         https://cgilab.nctu.edu.tw/
 */

#pragma once
#include <string>
#include <random>
#include <sstream>
#include <map>
#include <iostream>
#include <type_traits>
#include <algorithm>
#include <fstream>
#include "board.h"
#include "action.h"
#include "weight.h"

class agent {
public:
	agent(const std::string& args = "") {
		std::stringstream ss("name=unknown role=unknown " + args);
		for (std::string pair; ss >> pair; ) {
			std::string key = pair.substr(0, pair.find('='));
			std::string value = pair.substr(pair.find('=') + 1);
			meta[key] = { value };
		}
	}
	virtual ~agent() {}
	virtual void open_episode(const std::string& flag = "") {}
	virtual void close_episode(const std::string& flag = "") {}
	virtual action take_action(const board& b) { return action(); }
	virtual bool check_for_win(const board& b) { return false; }

public:
	virtual std::string property(const std::string& key) const { return meta.at(key); }
	virtual void notify(const std::string& msg) { meta[msg.substr(0, msg.find('='))] = { msg.substr(msg.find('=') + 1) }; }
	virtual std::string name() const { return property("name"); }
	virtual std::string role() const { return property("role"); }

protected:
	typedef std::string key;
	struct value {
		std::string value;
		operator std::string() const { return value; }
		template<typename numeric, typename = typename std::enable_if<std::is_arithmetic<numeric>::value, numeric>::type>
		operator numeric() const { return numeric(std::stod(value)); }
	};
	std::map<key, value> meta;
};

/**
 * base agent for agents with randomness
 */
class random_agent : public agent {
public:
	random_agent(const std::string& args = "") : agent(args) {
		if (meta.find("seed") != meta.end())
			engine.seed(int(meta["seed"]));
	}
	virtual ~random_agent() {}

protected:
	std::default_random_engine engine;
};

/**
 * base agent for agents with weight tables and a learning rate
 */
class tuple_agent : public agent {
public:
	tuple_agent(const std::string& args = "") : agent("name=tuple role=player " + args), alpha(0) {
		if (meta.find("init") != meta.end())
			init_weights(meta["init"]);
		if (meta.find("load") != meta.end())
			load_weights(meta["load"]);
		if (meta.find("alpha") != meta.end())
			alpha = float(meta["alpha"]);
	}
	virtual ~tuple_agent() {
		if (meta.find("save") != meta.end())
			save_weights(meta["save"]);
	}

	virtual action take_action(const board& before){
		int best_op = -1;
		int best_reward = -1;
		float best_value = -1000000;

		board best_after;

		for(int op = 0; op <= 3; op++){

			board after = before;
			int reward = after.slide(op);

			if(reward == -1){
				continue;
			}

			float value = calculate_value(after);

			if((reward + value) > (best_reward + best_value)){
				best_op = op;
				best_after = after;
				best_value = value;
				best_reward = reward;
			}
		}

		if(best_op != -1){
			record.push_back({best_reward, best_after});
		}

		return action::slide(best_op);
	}

	virtual void open_episode(const std::string& flag = ""){
		record.clear();
	}

	virtual void close_episode(const std::string& flag = ""){
		if(record.empty()){
			return;
		}

		if(alpha == 0){
			return;
		}

		adjust_value(record[record.size() - 1].after, 0);

		for(int i = record.size() - 2; i >= 0; i--){
			float adjust_target = record[i+1].reward + calculate_value(record[i+1].after);
			adjust_value(record[i].after, adjust_target);
		}
	}

	struct step{
		int reward;
		board after;
	};

	std::vector<step> record;

	// 4 x 8 Tuple
	//
	// int get_feature(const board& after, int vertice1, int vertice2, int vertice3, int vertice4) const{
	// 	return after(vertice1) * 16 * 16 * 16 + after(vertice2) * 16 * 16 + after(vertice3) * 16 + after(vertice4);
	// }

	// float calculate_value(const board& after) const{
	// 	float value = 0;

	// 	value += net[0][get_feature(after,  0,  1,  2,  3)];
	// 	value += net[1][get_feature(after,  4,  5,  6,  7)];
	// 	value += net[2][get_feature(after,  8,  9, 10, 11)];
	// 	value += net[3][get_feature(after, 12, 13, 14, 15)];

	// 	value += net[4][get_feature(after, 0, 4,  8, 12)];
	// 	value += net[5][get_feature(after, 1, 5,  9, 13)];
	// 	value += net[6][get_feature(after, 2, 6, 10, 14)];
	// 	value += net[7][get_feature(after, 3, 7, 11, 15)];

	// 	return value;
	// }

	// void adjust_value(const board& after, float target){
	// 	float current = calculate_value(after);
	// 	float offset = target - current;
	// 	float adjust = alpha * offset;

	// 	net[0][get_feature(after,  0,  1,  2,  3)] += adjust;
	// 	net[1][get_feature(after,  4,  5,  6,  7)] += adjust;
	// 	net[2][get_feature(after,  8,  9, 10, 11)] += adjust;
	// 	net[3][get_feature(after, 12, 13, 14, 15)] += adjust;

	// 	net[4][get_feature(after, 0, 4,  8, 12)] += adjust;
	// 	net[5][get_feature(after, 1, 5,  9, 13)] += adjust;
	// 	net[6][get_feature(after, 2, 6, 10, 14)] += adjust;
	// 	net[7][get_feature(after, 3, 7, 11, 15)] += adjust;
	// }

	// 6 x 8 Tuple
	int get_feature(const board& after, int vertice1, int vertice2, int vertice3, int vertice4, int vertice5, int vertice6) const{
		return after(vertice1) * 16 * 16 * 16 * 16 * 16 + after(vertice2) * 16 * 16 * 16 * 16 + after(vertice3) * 16 * 16 * 16 + after(vertice4) * 16 * 16 + after(vertice5) * 16 + after(vertice6);
	}

	float calculate_value(const board& after) const{
		float value = 0;

		// OO
		// OO
		// OO
		//
		value += net[0][get_feature(after, 0, 1, 2, 4, 5, 6)];
		value += net[1][get_feature(after, 1, 2, 3, 5, 6, 7)];
		value += net[2][get_feature(after, 8, 9, 10, 12, 13, 14)];
		value += net[3][get_feature(after, 9, 10, 11, 13, 14, 15)];

		value += net[4][get_feature(after, 0, 1, 4, 5, 8, 9)];
		value += net[5][get_feature(after, 2, 3, 6, 7, 10, 11)];
		value += net[6][get_feature(after, 4, 5, 8, 9, 12, 13)];
		value += net[7][get_feature(after, 6, 7, 10, 11, 14, 15)];

		// OO
		// OO
		// OO (at mid)
		//
		value += net2[0][get_feature(after, 1, 2, 5, 6, 9, 10)];
		value += net2[1][get_feature(after, 5, 6, 9, 10, 13, 14)];
		value += net2[2][get_feature(after, 4, 5, 6, 8, 9, 10)];
		value += net2[3][get_feature(after, 5, 6, 7, 9, 10, 11)];

		value += net2[4][get_feature(after, 1, 2, 5, 6, 9, 10)];
		value += net2[5][get_feature(after, 5, 6, 9, 10, 13, 14)];
		value += net2[6][get_feature(after, 4, 5, 6, 8, 9, 10)];
		value += net2[7][get_feature(after, 5, 6, 7, 9, 10, 11)];

		// OO
		// OO
		// O
		// O
		//
		value += net3[0][get_feature(after, 0, 1, 4, 5, 8, 12)];
		value += net3[1][get_feature(after, 0, 1, 2, 3, 6, 7)];
		value += net3[2][get_feature(after, 3, 7, 10, 11, 14, 15)];
		value += net3[3][get_feature(after, 8, 9, 12, 13, 14, 15)];

		value += net3[4][get_feature(after, 0, 4, 8, 9, 12, 13)];
		value += net3[5][get_feature(after, 10, 11, 12, 13, 14, 15)];
		value += net3[6][get_feature(after, 2, 3, 6, 7, 11, 15)];
		value += net3[7][get_feature(after, 0, 1, 2, 3, 4, 5)];

		// OO
		// OO
		// O
		// O  (at mid)
		//
		value += net4[0][get_feature(after, 1, 2, 5, 6, 9, 13)];
		value += net4[1][get_feature(after, 4, 5, 6, 7, 10, 11)];
		value += net4[2][get_feature(after, 2, 6, 9, 10, 13, 14)];
		value += net4[3][get_feature(after, 4, 5, 8, 9, 10, 11)];

		value += net4[4][get_feature(after, 1, 5, 9, 10, 13, 14)];
		value += net4[5][get_feature(after, 6, 7, 8, 9, 10, 11)];
		value += net4[6][get_feature(after, 1, 2, 5, 6, 10, 14)];
		value += net4[7][get_feature(after, 4, 5, 6, 7, 8, 9)];

		return value;
	}

	void adjust_value(const board& after, float target){
		float current = calculate_value(after);
		float offset = target - current;
		float adjust = alpha * offset;

		net[0][get_feature(after, 0, 1, 2, 4, 5, 6)] += adjust;
		net[1][get_feature(after, 1, 2, 3, 5, 6, 7)] += adjust;
		net[2][get_feature(after, 8, 9, 10, 12, 13, 14)] += adjust;
		net[3][get_feature(after, 9, 10, 11, 13, 14, 15)] += adjust;

		net[4][get_feature(after, 0, 1, 4, 5, 8, 9)] += adjust;
		net[5][get_feature(after, 2, 3, 6, 7, 10, 11)] += adjust;
		net[6][get_feature(after, 4, 5, 8, 9, 12, 13)] += adjust;
		net[7][get_feature(after, 6, 7, 10, 11, 14, 15)] += adjust;


		net2[0][get_feature(after, 1, 2, 5, 6, 9, 10)] += adjust;
		net2[1][get_feature(after, 5, 6, 9, 10, 13, 14)] += adjust;
		net2[2][get_feature(after, 4, 5, 6, 8, 9, 10)] += adjust;
		net2[3][get_feature(after, 5, 6, 7, 9, 10, 11)] += adjust;

		net2[4][get_feature(after, 1, 2, 5, 6, 9, 10)] += adjust;
		net2[5][get_feature(after, 5, 6, 9, 10, 13, 14)] += adjust;
		net2[6][get_feature(after, 4, 5, 6, 8, 9, 10)] += adjust;
		net2[7][get_feature(after, 5, 6, 7, 9, 10, 1)] += adjust;


		net3[0][get_feature(after, 0, 1, 4, 5, 8, 12)] += adjust;
		net3[1][get_feature(after, 0, 1, 2, 3, 6, 7)] += adjust;
		net3[2][get_feature(after, 3, 7, 10, 11, 14, 15)] += adjust;
		net3[3][get_feature(after, 8, 9, 12, 13, 14, 15)] += adjust;

		net3[4][get_feature(after, 0, 4, 8, 9, 12, 13)] += adjust;
		net3[5][get_feature(after, 10, 11, 12, 13, 14, 15)] += adjust;
		net3[6][get_feature(after, 2, 3, 6, 7, 11, 15)] += adjust;
		net3[7][get_feature(after, 0, 1, 2, 3, 4, 5)] += adjust;


		net4[0][get_feature(after, 1, 2, 5, 6, 9, 13)] += adjust;
		net4[1][get_feature(after, 4, 5, 6, 7, 10, 11)] += adjust;
		net4[2][get_feature(after, 2, 6, 9, 10, 13, 14)] += adjust;
		net4[3][get_feature(after, 4, 5, 8, 9, 10, 11)] += adjust;

		net4[4][get_feature(after, 1, 5, 9, 10, 13, 14)] += adjust;
		net4[5][get_feature(after, 6, 7, 8, 9, 10, 11)] += adjust;
		net4[6][get_feature(after, 1, 2, 5, 6, 10, 14)] += adjust;
		net4[7][get_feature(after, 4, 5, 6, 7, 8, 9)] += adjust;
	}

protected:
	virtual void init_weights(const std::string& info) {
		std::string res = info; // comma-separated sizes, e.g., "65536,65536"
		for (char& ch : res)
			if (!std::isdigit(ch)) ch = ' ';
		std::stringstream in(res);
		for (size_t size; in >> size; net.emplace_back(size));
	}
	virtual void load_weights(const std::string& path) {
		std::ifstream in(path, std::ios::in | std::ios::binary);
		if (!in.is_open()) std::exit(-1);
		uint32_t size;
		in.read(reinterpret_cast<char*>(&size), sizeof(size));
		net.resize(size);
		for (weight& w : net) in >> w;
		in.close();
	}
	virtual void save_weights(const std::string& path) {
		std::ofstream out(path, std::ios::out | std::ios::binary | std::ios::trunc);
		if (!out.is_open()) std::exit(-1);
		uint32_t size = net.size();
		out.write(reinterpret_cast<char*>(&size), sizeof(size));
		for (weight& w : net) out << w;
		out.close();
	}

protected:
	// 16 ^ 6
	//std::vector<weight> net {16777216, 16777216, 16777216, 16777216, 16777216, 16777216, 16777216, 16777216};
	std::vector<weight> net {16777216, 16777216, 16777216, 16777216, 16777216, 16777216, 16777216, 16777216};
	std::vector<weight> net2 {16777216, 16777216, 16777216, 16777216, 16777216, 16777216, 16777216, 16777216};
	std::vector<weight> net3 {16777216, 16777216, 16777216, 16777216, 16777216, 16777216, 16777216, 16777216};
	std::vector<weight> net4 {16777216, 16777216, 16777216, 16777216, 16777216, 16777216, 16777216, 16777216};
	float alpha;
};

/**
 * default random environment, i.e., placer
 * place the hint tile and decide a new hint tile
 */
class random_placer : public random_agent {
public:
	random_placer(const std::string& args = "") : random_agent("name=place role=placer " + args) {
		spaces[0] = { 12, 13, 14, 15 };
		spaces[1] = { 0, 4, 8, 12 };
		spaces[2] = { 0, 1, 2, 3};
		spaces[3] = { 3, 7, 11, 15 };
		spaces[4] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
	}

	virtual action take_action(const board& after) {
		std::vector<int> space = spaces[after.last()];
		std::shuffle(space.begin(), space.end(), engine);
		for (int pos : space) {
			if (after(pos) != 0) continue;

			int bag[3], num = 0;
			for (board::cell t = 1; t <= 3; t++)
				for (size_t i = 0; i < after.bag(t); i++)
					bag[num++] = t;
			std::shuffle(bag, bag + num, engine);

			board::cell tile = after.hint() ?: bag[--num];
			board::cell hint = bag[--num];

			return action::place(pos, tile, hint);
		}
		return action();
	}

private:
	std::vector<int> spaces[5];
};

/**
 * random player, i.e., slider
 * select a legal action randomly
 */
class random_slider : public random_agent {
public:
	random_slider(const std::string& args = "") : random_agent("name=random_slide role=slider " + args),
		opcode({ 0, 1, 2, 3 }) {}

	virtual action take_action(const board& before) {
		std::shuffle(opcode.begin(), opcode.end(), engine);
		for (int op : opcode) {
			board::reward reward = board(before).slide(op);
			if (reward != -1) return action::slide(op);
		}
		return action();
	}

private:
	std::array<int, 4> opcode;
};
