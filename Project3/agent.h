/**
 * Framework for NoGo and similar games (C++ 11)
 * agent.h: Define the behavior of variants of the player
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
#include <type_traits>
#include <algorithm>
#include <fstream>
#include <unistd.h>
#include <ctime>
#include "board.h"
#include "action.h"

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
		if (meta.find("seed") != meta.end()){
			engine.seed(int(meta["seed"]));
		}
	}
	virtual ~random_agent(){}

protected:
	std::default_random_engine engine;
};

/**
 * random player for both side
 * put a legal piece randomly
 */
class player : public random_agent {
public:
	player(const std::string& args = "") : random_agent("name=random role=unknown " + args),
		space(board::size_x * board::size_y), opp_space(board::size_x * board::size_y), who(board::empty){

		if (name().find_first_of("[]():; ") != std::string::npos){
			throw std::invalid_argument("invalid name: " + name());
		}
		if (role() == "black"){
			who = board::black;
		}
		if (role() == "white"){
			who = board::white;
		}
		if (who == board::empty){
			throw std::invalid_argument("invalid role: " + role());
		}
		for (size_t i = 0; i < space.size(); i++){
			space[i] = action::place(i, who);

			if(who == board::black){
				opp_space[i] = action::place(i, board::white);
			}
 			if(who == board::white){
				opp_space[i] = action::place(i, board::black);
			}
		}
	}

	virtual action take_action(const board& state) {
		node* root = new_node(state);
		simulation_count = stoi(property("N"));
		weight = stof(property("c"));
		
		std::clock_t start_time = std::clock(); // get current time

		while(total_count<simulation_count){
			our_turn = true;
			update_nodes.push_back(root);
			insert(root,state);
		}

		// while(1){
		// 	our_turn = true;
		// 	update_nodes.push_back(root);
		// 	insert(root,state);
		// 	if( (std::clock() - start_time)/ (double) CLOCKS_PER_SEC > 1) {
		// 		break;
		// 	}
		// }

 		total_count = 0;

		if(root->childs.size() == 0){
			return action();
		}

		//find the best child 
 		int index = -1;
 		float max =- 100;

		for(size_t i = 0 ; i <root->childs.size(); i++){
 			if(root->childs[i]->visit_count > max){
 				max = root->childs[i]->visit_count;
 				index = i;
 			}
 		}

		for (const action::place& move : space) {
			board after = state;
			if (move.apply(after) == board::legal){
				if(after == root->childs[index]->state){
					delete_node(root);
					return move;
				}
			}
		}
		delete_node(root);
		return action();
	}

	struct node{
 		board state;
 		float visit_count;
 		float win_count;
 		float uct_value;
 		std::vector<node*> childs;
 	};

	struct node* new_node(board state){
 		struct node* current_node = new struct node;
 		current_node->visit_count = 0;
 		current_node->win_count = 0;
 		current_node->uct_value = 10000;
 		current_node->state = state;
 		return current_node;
 	}

	bool simulation(struct node * current_node){
 		board after = current_node->state;
 		bool end = false;
 		bool win = true;
 		int count = 0 ;

		if(our_turn == true) {
 			win = false;
 			count = 0;
 		}
		else{
			win = true;
 			count = 1;
		}

 		while(!end){
 			bool exist_legal_move = false;

			// our move
 			if(count % 2 == 0 ){
 				std::shuffle(space.begin(), space.end(), engine);
 				for (const action::place& move : space){
 					if (move.apply(after) == board::legal){
 						exist_legal_move = true;
 						count++; 
						win = true;
 						break;
 					}
 				}
 			}
			// the opponent's move
 			else if(count % 2 == 1 ){
 				std::shuffle(opp_space.begin(), opp_space.end(), engine);
 				for (const action::place& move : opp_space){
 					if (move.apply(after) == board::legal){
 						exist_legal_move = true;
 						count++; 
						win = false;
 						break;
 					}
 				}
 			}

 			if(!exist_legal_move){
				end = true;
			}
 		}

 		total_count++;
 		return win;
 	}

	void insert(struct node* root, board state){
 		// collect child
 		size_t number_of_legal_move = 0;

 		if(our_turn == true){
 			for (const action::place& move : space) {
 				board after = state;

 				if (move.apply(after) == board::legal){
 					if(root->childs.size() <= number_of_legal_move++){
 						struct node * current_node = new_node(after);		
 						root->childs.push_back(current_node);
 					}
 				}
 			}
 		}
 		else{
 			for (const action::place& move : opp_space) {
 				board after = state;
 				if (move.apply(after) == board::legal){
 					if(root->childs.size() <= number_of_legal_move++){
 						struct node * current_node = new_node(after);		
 						root->childs.push_back(current_node);
 					}
 				}
 			}
 		}

 		// do simulation
 		if(root->visit_count == 0) {
 			bool win = simulation(root);
 			update(win);
 		}
 		else{
 			int index = -1;
 			float max =- 100;
 			size_t child_visit_count = 0;
 			bool do_expand = true;

 			//get number of child have been visited
 			for(size_t i = 0 ; i < root->childs.size(); i++) {
 				if(root->childs[i]->visit_count != 0)
 					child_visit_count++;
 			}

 			// check need expand or not
 			if(child_visit_count == number_of_legal_move){
				do_expand = false;
			}

 			if(number_of_legal_move == 0){
 				bool win = simulation(root);
 				update(win);
 				return;
 			} 

 			if(do_expand){
				std::shuffle(root->childs.begin(), root->childs.end(), engine);

 				for(size_t i = 0 ; i < root->childs.size(); i++){
 					if(root->childs[i]->uct_value > max && root->childs[i]->visit_count == 0){
 						max = root->childs[i]->uct_value;
 						index = i;
 					}
 				}
 			}
			else{
 				for(size_t i = 0 ; i < root->childs.size(); i++){
 					if(root->childs[i]->uct_value > max){
 						max = root->childs[i]->uct_value;
 						index = i;
 					}
 				}
 			}
			our_turn = !our_turn;
 			update_nodes.push_back(root->childs[index]);
 			insert(root->childs[index],root->childs[index]->state);
 		}
 	}

	void update(bool win){
 		float value = 0;

 		if(win){
			value = 1;
		}
		
 		for(size_t i = 0 ; i < update_nodes.size() ; i++){
 			update_nodes[i]->visit_count++;
 			update_nodes[i]->win_count += value;	
 			update_nodes[i]->uct_value = (update_nodes[i]->win_count / update_nodes[i]->visit_count) + weight * log(total_count) / update_nodes[i]->visit_count;		
 		}

 		// clear total_count and update_nodes
 		update_nodes.clear();
 	}

	void delete_node(struct node * root){
 		for(size_t i = 0 ; i < root->childs.size(); i++)
 			delete_node(root->childs[i]);
 		delete(root);
 	}

	bool our_turn;
	float total_count = 0.0;
	std::vector<node*> update_nodes;

private:
	std::vector<action::place> space;
	board::piece_type who;

	std::vector<action::place> opp_space;

	float visit_count;
	float win_count;
	int simulation_count;
	float weight;
};
