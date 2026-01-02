#include <cstdint>
#include <iostream>
#include <vector>
#include <sstream>
#include <array>
#include <ostream>
#include <stdexcept>

#define NUM_FLIPS 7
#define MAX_ROUNDS 10
#define TARGET_SCORE 200
#define MAX_CARD 12
#define DEBUG false

uint64_t TOTAL_STATES;
uint64_t SOLVED_STATES;
int LAST_PCT;
int TOTAL_CARDS;
float *EXPECTED_ROUNDS_ARRAY = nullptr;

uint64_t features_to_index(int round, int score, std::array<int, (NUM_FLIPS - 1)> cards) {
    uint64_t index = round;
    index = index * TARGET_SCORE + score;
    for (int i = 0; i < NUM_FLIPS - 1; i++) {
        index = index * MAX_CARD + (cards[i] - 1);
    }
    return index;
}

void index_to_features(uint64_t index, int &round, int &score, std::array<int, (NUM_FLIPS - 1)> &cards) {
    for (int i = NUM_FLIPS - 2; i >= 0; i--) {
        cards[i] = (index % MAX_CARD) + 1;
        index /= MAX_CARD;
    }
    score = (index % TARGET_SCORE);
    index /= TARGET_SCORE;
    round = index;
}


// Returns the expected rounds for a given state
float calculate_expected_rounds(uint64_t index, int round, int score,
                                std::array<int, (NUM_FLIPS - 1)> cards, int drawn_cards,
                                float &fold_expected_rounds, float &draw_expected_rounds) {
    // std::cout << index << std::endl;
    // Initialize dummy variables
    float dummy_fold, dummy_draw;

    // Check that the index and features match
    if (DEBUG) {
        uint64_t check_index = features_to_index(round, score, cards);
        int check_round, check_score;
        std::array<int, (NUM_FLIPS - 1)> check_cards;
        index_to_features(index, check_round, check_score, check_cards);
        if (index >= TOTAL_STATES || check_index != index || check_round != round || check_score != score || check_cards != cards) {
            std::ostringstream ss;
            ss << "Index: " << index << " vs " << check_index << std::endl
            << "Round: " << round << " vs " << check_round << std::endl
            << "Score: " << score << " vs " << check_score << std::endl
            << "Cards: ";
            for (int i = 0; i < NUM_FLIPS - 1; i++) {
                if (i == 0) {
                    ss << "(" << cards[i];
                }
                else if (i < NUM_FLIPS - 2) {
                    ss << " " << cards[i];
                }
                else {
                    ss << " " << cards[i] << ")";
                }
            }
            ss << " vs ";
            for (int i = 0; i < NUM_FLIPS - 1; i++) {
                if (i == 0) {
                    ss << "(" << check_cards[i];
                }
                else if (i < NUM_FLIPS - 2) {
                    ss << " " << check_cards[i];
                }
                else {
                    ss << " " << check_cards[i] << ")";
                }
            }
            ss << std::endl;
            throw std::runtime_error(ss.str());
        }
    }
    
    // Calculate expected rounds for folding
    // Check if this is the last round
    if (round == MAX_ROUNDS - 1) {
        fold_expected_rounds = static_cast<float>(MAX_ROUNDS);
    }
    // Otherwise, advance to the next round
    else {
        // Calculate next round
        int next_round = round + 1;
        // Calculate next score
        int next_score = score;
        for (int i = 0; i < drawn_cards; i++) {
            next_score += cards[i];
        }
        // Check if the target score is reached
        if (next_score >= TARGET_SCORE) {
            fold_expected_rounds = static_cast<float>(round);
        }
        // Otherwise, calculate expected rounds for the next state
        else {
            // Calculate next index
            std::array<int, (NUM_FLIPS - 1)> next_cards;
            for (int i = 0; i < (NUM_FLIPS - 1); i++) {
                next_cards[i] = 1;
            }
            uint64_t next_index = features_to_index(next_round, next_score, next_cards);
            // Find expected rounds
            float next_expected_rounds;
            if (EXPECTED_ROUNDS_ARRAY[next_index] < 0.0f) {
                next_expected_rounds = calculate_expected_rounds(next_index, next_round, next_score, next_cards, 0, dummy_fold, dummy_draw);
            }
            else {
                next_expected_rounds = EXPECTED_ROUNDS_ARRAY[next_index];
            }
            // Set fold expected rounds
            fold_expected_rounds = next_expected_rounds;
        }
    }

    // Calculate expected rounds for drawing another card
    draw_expected_rounds = 0.0f;
    // Iterate through each card that could be drawn
    for (int draw = 2; draw <= MAX_CARD; draw++) {
        // Calculate the probability of this draw
        int already_present = 0;
        for (int i = 0; i < drawn_cards; i++) {
            if (cards[i] == draw) {
                already_present++;
            }
        }
        float draw_probability = static_cast<float>((draw == 2 ? 3 : draw) - already_present) / static_cast<float>(TOTAL_CARDS - drawn_cards);
        // Check if a bust
        if (already_present && (draw > 2)) {
            // Check if this is the last round
            if (round == MAX_ROUNDS - 1) {
                draw_expected_rounds += draw_probability * static_cast<float>(MAX_ROUNDS);
            }
            // Otherwise, advance to the next round
            else {
                // Calculate next round
                int next_round = round + 1;
                // Get next index
                std::array<int, (NUM_FLIPS - 1)> next_cards;
                for (int i = 0; i < (NUM_FLIPS - 1); i++) {
                    next_cards[i] = 1;
                }
                uint64_t next_index = features_to_index(next_round, score, next_cards);
                // Find expected rounds
                float next_expected_rounds;
                if (EXPECTED_ROUNDS_ARRAY[next_index] < 0.0f) {
                    next_expected_rounds = calculate_expected_rounds(next_index, next_round, score, next_cards, 0, dummy_fold, dummy_draw);
                }
                else {
                    next_expected_rounds = EXPECTED_ROUNDS_ARRAY[next_index];
                }
                // Add to expected rounds
                draw_expected_rounds += draw_probability * next_expected_rounds;
            }
        }
        // Otherwise, it is a safe draw
        else {
            // Check if this is the last card
            if (drawn_cards == NUM_FLIPS - 1) {
                // Calculate the next score
                int next_score = score;
                for (int i = 0; i < NUM_FLIPS - 1; i++) {
                    next_score += cards[i];
                }
                next_score += (draw + 15);
                // Check if the target score is reached
                if (next_score >= TARGET_SCORE) {
                    draw_expected_rounds += draw_probability * static_cast<float>(round);
                }
                // Check if this is the last round
                else if (round == MAX_ROUNDS - 1) {
                    draw_expected_rounds += draw_probability * static_cast<float>(MAX_ROUNDS);
                }
                // Otherwise, advance to the next round
                else {
                    // Calculate next round
                    int next_round = round + 1;
                    // Get next index
                    std::array<int, (NUM_FLIPS - 1)> next_cards;
                    for (int i = 0; i < (NUM_FLIPS - 1); i++) {
                        next_cards[i] = 1;
                    }
                    uint64_t next_index = features_to_index(next_round, next_score, next_cards);
                    // Find expected rounds
                    float next_expected_rounds;
                    if (EXPECTED_ROUNDS_ARRAY[next_index] < 0.0f) {
                        next_expected_rounds = calculate_expected_rounds(next_index, next_round, next_score, next_cards, 0, dummy_fold, dummy_draw);
                    }
                    else {
                        next_expected_rounds = EXPECTED_ROUNDS_ARRAY[next_index];
                    }
                    // Add to expected rounds
                    draw_expected_rounds += draw_probability * next_expected_rounds;
                }
            }
            // Otherwise, continue drawing (if desired)
            else {
                // Create next cards array
                std::array<int, (NUM_FLIPS - 1)> next_cards = cards;
                next_cards[drawn_cards] = draw;
                // Calculate next index
                uint64_t next_index = features_to_index(round, score, next_cards);
                // Find expected rounds
                float next_expected_rounds;
                if (EXPECTED_ROUNDS_ARRAY[next_index] < 0.0f) {
                    next_expected_rounds = calculate_expected_rounds(next_index, round, score, next_cards, drawn_cards + 1, dummy_fold, dummy_draw);
                }
                else {
                    next_expected_rounds = EXPECTED_ROUNDS_ARRAY[next_index];
                }
                // Add to expected rounds
                draw_expected_rounds += draw_probability * next_expected_rounds;
            }
        }
    }

    // Choose whichever option has fewer rounds
    float min_expected_rounds = std::min(fold_expected_rounds, draw_expected_rounds);
    EXPECTED_ROUNDS_ARRAY[index] = min_expected_rounds;
    SOLVED_STATES++;
    float pct_solved = static_cast<float>(SOLVED_STATES) / static_cast<float>(TOTAL_STATES);
    int pct_solved_int = static_cast<int>(pct_solved * 100.0f);
    if (pct_solved_int != LAST_PCT) {
        LAST_PCT = pct_solved_int;
        std::cout << pct_solved_int << " ";
    }
    return min_expected_rounds;
}


int main()
{
    // Calculate total number of states and cards
    TOTAL_STATES = MAX_ROUNDS * TARGET_SCORE;
    for (int i = 0; i < NUM_FLIPS - 1; i++) {
        TOTAL_STATES *= MAX_CARD;
    }
    SOLVED_STATES = 0;
    LAST_PCT = 0;
    TOTAL_CARDS = 0;
    for (int card = 2; card <= MAX_CARD; card++) {
        TOTAL_CARDS += (card == 2 ? 3 : card);
    }
    std::cout << "Total states: " << TOTAL_STATES << std::endl;

    // for (uint64_t i = 0; i < TOTAL_STATES; i++) {
    //     int round, score;
    //     std::array<int, 6> cards;
    //     index_to_features(i, round, score, cards);
    //     uint64_t index = features_to_index(round, score, cards);
    //     if (index != i) {
    //         std::cerr << "Error in index conversion at index " << i << std::endl;
    //         return 1;
    //     }
    // }
    
    // Initialize the array of expected rounds
    EXPECTED_ROUNDS_ARRAY = new float[TOTAL_STATES];
    for (uint64_t i = 0; i < TOTAL_STATES; i++) {
        EXPECTED_ROUNDS_ARRAY[i] = -1.0f;
    }

    // Calculate expected rounds for the start state
    int start_round = 0;
    int start_score = 0;
    std::array<int, (NUM_FLIPS - 1)> start_cards;
    for (int i = 0; i < (NUM_FLIPS - 1); i++) {
        start_cards[i] = 1;
    }
    uint64_t start_index = features_to_index(start_round, start_score, start_cards);
    float expected_rounds, draw_expected_rounds, fold_expected_rounds;
    try {
        expected_rounds = calculate_expected_rounds(start_index, start_round, start_score, start_cards, 0, fold_expected_rounds, draw_expected_rounds);
        for (int alt_start_score = 1; alt_start_score < TARGET_SCORE; alt_start_score++) {
            uint64_t alt_start_index = features_to_index(start_round, alt_start_score, start_cards);
            float alt_expected_rounds = calculate_expected_rounds(alt_start_index, start_round, alt_start_score, start_cards, 0, fold_expected_rounds, draw_expected_rounds);
        }
    }
    catch (const std::runtime_error &e) {
        std::cerr << "Error in index conversion for start state:\n" << e.what() << std::endl;
        return 1;
    }
    std::cout << "\nExpected rounds from start state: " << expected_rounds << std::endl;

    // Interactive query loop
    while (true) {
        std::cout << "\nEnter score and cards (or 'q' to quit): ";
        std::string line;
        std::getline(std::cin, line);

        if (line == "q") {
            break;
        }

        std::istringstream iss(line);
        int query_score;
        std::vector<int> cards_input;

        // Parse score
        if (!(iss >> query_score)) {
            continue;
        }

        // Parse cards
        int card;
        while (iss >> card) {
            if ((card >= 0) && (card <= 2)) {
                card = 2;
            }
            cards_input.push_back(card);
        }

        // Validate
        if (query_score < 0 || query_score >= TARGET_SCORE) {
            continue;
        }
        if (cards_input.size() > NUM_FLIPS - 1) {
            continue;
        }
        bool valid_cards = true;
        for (int c : cards_input) {
            if (c < 2 || c > MAX_CARD) {
                valid_cards = false;
                break;
            }
        }
        if (!valid_cards) {
            continue;
        }

        // Build cards array
        std::array<int, (NUM_FLIPS - 1)> query_cards;
        for (int i = 0; i < NUM_FLIPS - 1; i++) {
            if (i < cards_input.size()) {
                query_cards[i] = cards_input[i];
            } else {
                query_cards[i] = 1;
            }
        }

        // Calculate index and get expected rounds
        int query_round = 0;
        uint64_t query_index = features_to_index(query_round, query_score, query_cards);
        float query_fold_exp, query_draw_exp;
        calculate_expected_rounds(query_index, query_round, query_score, query_cards, cards_input.size(), query_fold_exp, query_draw_exp);

        std::cout << "Fold expected rounds: " << query_fold_exp << std::endl;
        std::cout << "Draw expected rounds: " << query_draw_exp << std::endl;
    }

    // Delete the array
    delete[] EXPECTED_ROUNDS_ARRAY;
    return 0;
}
