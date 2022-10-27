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
			std::cout << "i = " << i << std::endl;
			float adjust_target = record[i+1].reward + calculate_value(record[i+1].after);
			adjust_value(record[i].after, adjust_target);
		}
	}

	struct step{
		int reward;
		board after;
	};

	std::vector<step> record;

	int get_feature(const board& after, int vertice1, int vertice2, int vertice3, int vertice4) const{
		return after(vertice1) * 25 * 25 * 25 + after(vertice2) * 25 * 25 + after(vertice3) * 25 + after(vertice4);
	}

	float calculate_value(const board& after) const{
		float value = 0;

		value += net[0][get_feature(after, 0, 1, 2, 3)];
		value += net[1][get_feature(after,  4,  5,  6,  7)];
		value += net[2][get_feature(after,  8,  9, 10, 11)];
		value += net[3][get_feature(after, 12, 13, 14, 15)];

		value += net[4][get_feature(after, 0, 4,  8, 12)];
		value += net[5][get_feature(after, 1, 5,  9, 13)];
		value += net[6][get_feature(after, 2, 6, 10, 14)];
		value += net[7][get_feature(after, 3, 7, 11, 15)];

		return value;
	}

	void adjust_value(const board& after, float target){
		float current = calculate_value(after);
		float offset = target - current;
		float adjust = alpha * offset;


		net[0][get_feature(after,  0,  1,  2,  3)] += adjust;
		net[1][get_feature(after,  4,  5,  6,  7)] += adjust;
		net[2][get_feature(after,  8,  9, 10, 11)] += adjust;
		net[3][get_feature(after, 12, 13, 14, 15)] += adjust;

		net[4][get_feature(after, 0, 4,  8, 12)] += adjust;
		net[5][get_feature(after, 1, 5,  9, 13)] += adjust;
		net[6][get_feature(after, 2, 6, 10, 14)] += adjust;
		net[7][get_feature(after, 3, 7, 11, 15)] += adjust;
	}

protected:
	virtual void init_weights(const std::string& info) {
		// std::string res = info; // comma-separated sizes, e.g., "65536,65536"
		// for (char& ch : res)
		// 	if (!std::isdigit(ch)) ch = ' ';
		// std::stringstream in(res);
		// for (size_t size; in >> size; net.emplace_back(size));
		// net.emplace_back(65536);
		net.emplace_back(25 * 25 * 25 * 25);
		net.emplace_back(25 * 25 * 25 * 25);
		net.emplace_back(25 * 25 * 25 * 25);
		net.emplace_back(25 * 25 * 25 * 25);
		net.emplace_back(25 * 25 * 25 * 25);
		net.emplace_back(25 * 25 * 25 * 25);
		net.emplace_back(25 * 25 * 25 * 25);
		net.emplace_back(25 * 25 * 25 * 25);

		// std::cout << "Initial Net" << std::endl;
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
	std::vector<weight> net;
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
