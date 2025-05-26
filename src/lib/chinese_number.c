#include "chinese_number.h"

#include <bemapiset.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <uchar.h>
#include <math.h>
#include <stdbool.h>
#include <limits.h>
#include <windows.h>

// Convert from
// https://github.com/wenyan-lang/wenyan/blob/master/src/converts/hanzi2num.ts

typedef enum {
    TOKEN_TYPE_SIGN, // 負
    TOKEN_TYPE_DIGIT, // 一二三... 〇
    TOKEN_TYPE_DECIMAL, // ·
    TOKEN_TYPE_INT_MULT, // 十百千萬億...
    TOKEN_TYPE_FRAC_MULT, // 分釐毫...
    TOKEN_TYPE_DELIM, // 又 有
    TOKEN_TYPE_ZERO, // 零
    TOKEN_TYPE_BEGIN, // Pseudo-token
    TOKEN_TYPE_END, // Pseudo-token
    TOKEN_TYPE_UNKNOWN // For errors
} NumberTokenType;

typedef struct {
    NumberTokenType type;

    union {
        uint8_t digit; // For DIGIT, ZERO
        bool negative; // For SIGN
        int exp; // For INT_MULT, FRAC_MULT, DECIMAL (always 0)
    } value;
} NumberToken;

// Map Chinese characters to tokens
typedef struct {
    char32_t ch;
    NumberToken token;
} TokenMapEntry;

// Unified token table based on Javascript NUM_TOKENS
static const TokenMapEntry k_token_map[] = {
    {U'負', {TOKEN_TYPE_SIGN, {.negative = true}}},
    {U'·', {TOKEN_TYPE_DECIMAL, {.exp = 0}}}, // U+00B7 Middle Dot
    {U'又', {TOKEN_TYPE_DELIM, {0}}},
    {U'有', {TOKEN_TYPE_DELIM, {0}}},
    {U'零', {TOKEN_TYPE_ZERO, {.digit = 0}}},
    {U'〇', {TOKEN_TYPE_DIGIT, {.digit = 0}}}, // U+3007 Ideographic Number Zero
    {U'一', {TOKEN_TYPE_DIGIT, {.digit = 1}}},
    {U'二', {TOKEN_TYPE_DIGIT, {.digit = 2}}},
    {U'三', {TOKEN_TYPE_DIGIT, {.digit = 3}}},
    {U'四', {TOKEN_TYPE_DIGIT, {.digit = 4}}},
    {U'五', {TOKEN_TYPE_DIGIT, {.digit = 5}}},
    {U'六', {TOKEN_TYPE_DIGIT, {.digit = 6}}},
    {U'七', {TOKEN_TYPE_DIGIT, {.digit = 7}}},
    {U'八', {TOKEN_TYPE_DIGIT, {.digit = 8}}},
    {U'九', {TOKEN_TYPE_DIGIT, {.digit = 9}}},
    {U'兩', {TOKEN_TYPE_DIGIT, {.digit = 2}}},
    {U'十', {TOKEN_TYPE_INT_MULT, {.exp = 1}}},
    {U'百', {TOKEN_TYPE_INT_MULT, {.exp = 2}}},
    {U'千', {TOKEN_TYPE_INT_MULT, {.exp = 3}}},
    {U'萬', {TOKEN_TYPE_INT_MULT, {.exp = 4}}},
    {U'億', {TOKEN_TYPE_INT_MULT, {.exp = 8}}},
    {U'兆', {TOKEN_TYPE_INT_MULT, {.exp = 12}}},
    {U'京', {TOKEN_TYPE_INT_MULT, {.exp = 16}}},
    {U'垓', {TOKEN_TYPE_INT_MULT, {.exp = 20}}},
    {U'秭', {TOKEN_TYPE_INT_MULT, {.exp = 24}}},
    {U'穰', {TOKEN_TYPE_INT_MULT, {.exp = 28}}},
    {U'溝', {TOKEN_TYPE_INT_MULT, {.exp = 32}}},
    {U'澗', {TOKEN_TYPE_INT_MULT, {.exp = 36}}},
    {U'正', {TOKEN_TYPE_INT_MULT, {.exp = 40}}},
    {U'載', {TOKEN_TYPE_INT_MULT, {.exp = 44}}},
    {U'極', {TOKEN_TYPE_INT_MULT, {.exp = 48}}},
    {U'分', {TOKEN_TYPE_FRAC_MULT, {.exp = -1}}},
    {U'釐', {TOKEN_TYPE_FRAC_MULT, {.exp = -2}}},
    {U'毫', {TOKEN_TYPE_FRAC_MULT, {.exp = -3}}},
    {U'絲', {TOKEN_TYPE_FRAC_MULT, {.exp = -4}}},
    {U'忽', {TOKEN_TYPE_FRAC_MULT, {.exp = -5}}},
    {U'微', {TOKEN_TYPE_FRAC_MULT, {.exp = -6}}},
    {U'纖', {TOKEN_TYPE_FRAC_MULT, {.exp = -7}}},
    {U'沙', {TOKEN_TYPE_FRAC_MULT, {.exp = -8}}},
    {U'塵', {TOKEN_TYPE_FRAC_MULT, {.exp = -9}}},
    {U'埃', {TOKEN_TYPE_FRAC_MULT, {.exp = -10}}},
    {U'渺', {TOKEN_TYPE_FRAC_MULT, {.exp = -11}}},
    {U'漠', {TOKEN_TYPE_FRAC_MULT, {.exp = -12}}},
    // Financial digits - map to regular digits
    {U'壹', {TOKEN_TYPE_DIGIT, {.digit = 1}}},
    {U'貳', {TOKEN_TYPE_DIGIT, {.digit = 2}}},
    {U'參', {TOKEN_TYPE_DIGIT, {.digit = 3}}},
    {U'肆', {TOKEN_TYPE_DIGIT, {.digit = 4}}},
    {U'伍', {TOKEN_TYPE_DIGIT, {.digit = 5}}},
    {U'陸', {TOKEN_TYPE_DIGIT, {.digit = 6}}},
    {U'柒', {TOKEN_TYPE_DIGIT, {.digit = 7}}},
    {U'捌', {TOKEN_TYPE_DIGIT, {.digit = 8}}},
    {U'玖', {TOKEN_TYPE_DIGIT, {.digit = 9}}},
    // Financial units - map to regular units
    {U'拾', {TOKEN_TYPE_INT_MULT, {.exp = 1}}},
    {U'佰', {TOKEN_TYPE_INT_MULT, {.exp = 2}}},
    {U'仟', {TOKEN_TYPE_INT_MULT, {.exp = 3}}},
};

static NumberToken lookup_token(const char32_t ch) {
    for (size_t i = 0; i < sizeof k_token_map / sizeof *k_token_map; ++i) {
        if (k_token_map[i].ch == ch) return k_token_map[i].token;
    }
    return (NumberToken){TOKEN_TYPE_UNKNOWN};
}

/* ---------- very small UTF-8 → UTF-32 decoder --------------------------- */
/* returns number of codepoints written to out; -1 on error */
/* (modified slightly to handle potential allocation failures more gracefully) */
static int utf8_to_u32(const char* s, char32_t** out_buf) {
    size_t cap = 32, len = 0;
    if (!s) return -1;
    char32_t* out = malloc(cap * sizeof *out);
    if (!out) return -1;

    const unsigned char* p = (const unsigned char*)s;
    while (*p) {
        uint32_t cp = 0;
        size_t more = 0;

        if (*p < 0x80) { cp = *p++; } else if ((*p & 0xE0) == 0xC0 && p[1]) {
            cp = *p & 0x1F;
            more = 1;
        } else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) {
            cp = *p & 0x0F;
            more = 2;
        } else if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
            cp = *p & 0x07;
            more = 3;
        } else {
            free(out);
            return -1;
        } /* invalid leading byte or truncated sequence */

        p++; // Move past the first byte
        for (size_t i = 0; i < more; ++i) {
            if ((p[i] & 0xC0) != 0x80) {
                free(out);
                return -1;
            } // Invalid continuation byte
            cp = (cp << 6) | (p[i] & 0x3F);
        }
        p += more; // Move past continuation bytes

        // Check for overlong encodings or invalid code points (optional but good practice)
        if ((more == 1 && cp < 0x80) ||
            (more == 2 && cp < 0x800) ||
            (more == 3 && cp < 0x10000) ||
            (cp >= 0xD800 && cp <= 0xDFFF) || // Surrogates
            cp > 0x10FFFF) {
            // Out of Unicode range
            free(out);
            return -1;
        }


        if (len == cap) {
            cap *= 2;
            // Check for potential overflow if cap becomes huge
            if (cap < len || cap > SIZE_MAX / sizeof(*out)) {
                free(out);
                return -1;
            }
            char32_t* next_out = realloc(out, cap * sizeof *out);
            if (!next_out) {
                free(out);
                return -1;
            }
            out = next_out;
        }
        out[len++] = (char32_t)cp;
    }
    *out_buf = out;
    return (int)len;
}

/* ---------- Tokenizer ---------------------------------------------------- */
static NumberToken* tokenize(const char32_t* code, size_t len, size_t* token_count) {
    // Allocate space for tokens + BEGIN + END
    size_t cap = len + 2;
    NumberToken* tokens = malloc(cap * sizeof(NumberToken));
    if (!tokens) return NULL;

    size_t count = 0;
    tokens[count++] = (NumberToken){TOKEN_TYPE_BEGIN};

    for (size_t i = 0; i < len; ++i) {
        const NumberToken t = lookup_token(code[i]);
        if (t.type == TOKEN_TYPE_UNKNOWN) {
#ifdef VERBOSE
            fprintf(stderr, "Error: Unknown character U+%04X in input.\n", code[i]);
#endif
            free(tokens);
            return NULL; // Error on unknown character
        }
        // Simple dynamic array resize (could optimize)
        if (count >= cap - 1) {
            // Need space for current token + END
            cap = cap > 0 ? cap * 2 : 16; // Grow capacity
            if (cap < count || cap > SIZE_MAX / sizeof(*tokens)) {
                // Check overflow
                free(tokens);
                return NULL;
            }
            NumberToken* next_tokens = realloc(tokens, cap * sizeof(NumberToken));
            if (!next_tokens) {
                free(tokens);
                return NULL;
            }
            tokens = next_tokens;
        }
        tokens[count++] = t;
    }

    tokens[count++] = (NumberToken){TOKEN_TYPE_END};
    *token_count = count;
    return tokens;
}

/* ---------- Parser State & Helpers -------------------------------------- */

// Parser digit states (from JS eDigitState)
typedef enum {
    DIGIT_STATE_NONE, // <END>, ·
    DIGIT_STATE_MULT, // 微
    DIGIT_STATE_MULT_AMBIG, // 十 (ambiguous: ...十 or 一十)
    DIGIT_STATE_DIGIT, // 一
    DIGIT_STATE_DIGIT_WITH_ZERO, // 一...零, 零零， 零一...零,
    DIGIT_STATE_DELIM, // 又
    DIGIT_STATE_ZERO, // 零<END>, 零·, 零又, 零微, 零一
    DIGIT_STATE_SIGN, // 負
    DIGIT_STATE_ZERO_MULT_AMBIG // 零十 (ambiguous: 零一十 or 零十 or 〇十)
} DigitState;

// Multiplier stack state (from JS eMultState)
typedef enum {
    MULT_STATE_NONE, // Stack is empty
    MULT_STATE_FRAC, // Top is fractional multiplier (< 0)
    MULT_STATE_INT, // Top is integer multiplier (>= 0, < infinity)
    MULT_STATE_DONE // Stack has been marked done (contains only infinity)
} MultState;

#define MULT_STACK_INITIAL_CAP 8
#define RESULT_DIGITS_INITIAL_CAP 32
#define EXP_INFINITY INT_MAX // Use INT_MAX to represent infinity state in stack

// Multiplier stack structure
typedef struct {
    int* exps; // Array of exponents
    size_t count;
    size_t capacity;
    int total_exp_add; // Sum of exponents currently in stack
} MultStack;

// Intermediate parse result structure
typedef struct {
    bool negative; // +1 or -1
    int exp; // Exponent of the *lowest* digit (updated during parsing)
    uint8_t* digits; // Array of digits (0-9), lowest to highest significance
    int count; // Number of digits currently stored
    size_t capacity; // Allocated capacity of digits array
} ParseResult;

// Helper: Initialize multiplier stack
static bool mult_stack_init(MultStack* stack) {
    stack->exps = malloc(MULT_STACK_INITIAL_CAP * sizeof(int));
    if (!stack->exps) return false;
    stack->count = 0;
    stack->capacity = MULT_STACK_INITIAL_CAP;
    stack->total_exp_add = 0;
    return true;
}

// Helper: Free multiplier stack
static void mult_stack_free(MultStack* stack) {
    free(stack->exps);
    stack->exps = NULL;
    stack->count = 0;
    stack->capacity = 0;
    stack->total_exp_add = 0;
}

// Helper: Push exponent onto multiplier stack
static bool mult_stack_push(MultStack* stack, int exp) {
    if (stack->count == stack->capacity) {
        size_t new_capacity = stack->capacity > 0 ? stack->capacity * 2 : MULT_STACK_INITIAL_CAP;
        if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(int)) return false; // Overflow check
        int* new_exps = realloc(stack->exps, new_capacity * sizeof(int));
        if (!new_exps) return false;
        stack->exps = new_exps;
        stack->capacity = new_capacity;
    }
    stack->exps[stack->count++] = exp;
    if (exp != EXP_INFINITY) {
        // Don't add infinity to the total sum
        stack->total_exp_add += exp;
    }
    return true;
}

// Helper: Pop exponent from multiplier stack
static void mult_stack_pop(MultStack* stack) {
    if (stack->count > 0) {
        stack->count--;
        int popped_exp = stack->exps[stack->count];
        if (popped_exp != EXP_INFINITY) {
            stack->total_exp_add -= popped_exp;
        }
    }
}

static int mult_stack_total(const MultStack* stack) {
    return stack->total_exp_add;
}

// Helper: Get top exponent from multiplier stack
static int mult_stack_top(const MultStack* stack) {
    return (stack->count == 0) ? 0 : stack->exps[stack->count - 1];
}

// Helper: Clear multiplier stack
static void mult_stack_clear(MultStack* stack) {
    stack->count = 0;
    stack->total_exp_add = 0;
}

// Helper: Mark multiplier stack as done
static bool mult_stack_mark_done(MultStack* stack) {
    mult_stack_clear(stack);
    return mult_stack_push(stack, EXP_INFINITY); // Push infinity marker
}

// Helper: Get multiplier stack state
static MultState mult_stack_state(const MultStack* stack) {
    if (stack->count == 0) return MULT_STATE_NONE;
    if (stack->exps[0] == EXP_INFINITY) return MULT_STATE_DONE; // Check the marker
    if (stack->exps[0] < 0) return MULT_STATE_FRAC;
    return MULT_STATE_INT;
}

// Helper: Initialize parse result
static bool result_init(ParseResult* result) {
    result->digits = malloc(RESULT_DIGITS_INITIAL_CAP * sizeof(uint8_t));
    if (!result->digits) return false;
    result->negative = false;
    result->exp = 0; // Tracks exponent of lowest digit place added so far
    result->count = 0;
    result->capacity = RESULT_DIGITS_INITIAL_CAP;
    return true;
}

// Helper: Free parse result
static void result_free(ParseResult* result) {
    free(result->digits);
    result->digits = NULL;
    result->count = 0;
    result->capacity = 0;
}

// Helper: Push a single digit onto result
static bool result_push_digit(ParseResult* result, const char digit) {
    if (result->count == result->capacity) {
        const size_t new_capacity = result->capacity > 0 ? result->capacity * 2 : RESULT_DIGITS_INITIAL_CAP;
        if (new_capacity < result->capacity || new_capacity > SIZE_MAX / sizeof(uint8_t)) return false;
        // Overflow check
        char* new_digits = realloc(result->digits, new_capacity * sizeof(uint8_t));
        if (!new_digits) return false;
        result->digits = new_digits;
        result->capacity = new_capacity;
    }
    result->digits[result->count++] = digit;
    result->exp++;
    // Exponent remains unchanged when pushing single digits
    // as it represents the exponent of the lowest digit's place value
    return true;
}

// Helper: Set result's lowest exponent directly (use with care)
static void result_reset_exp(ParseResult* result, int new_exp) {
    // This is used when encountering the first fractional/decimal token
    // when parsing backwards. It sets the baseline exponent.
    result->exp = new_exp;
}


/* ---------- Core Parser (based on JS 'parse') --------------------------- */
static ParseResult* parse_tokens(const NumberToken* tokens, size_t token_count) {
    DigitState digit_state = DIGIT_STATE_NONE;
    MultStack stack;
    ParseResult* result = malloc(sizeof(ParseResult)); // Allocate result structure

    if (!result || !mult_stack_init(&stack) || !result_init(result)) {
        if (result) result_free(result);
        mult_stack_free(&stack); // Free stack if init failed after result alloc
        free(result);
        return NULL;
    }

    // Parse backwards, skipping END token (index token_count - 1)
    // Stop before BEGIN token (index 0)
    for (size_t i = token_count - 1; i > 0; --i) {
        const NumberToken* token = &tokens[i - 1]; // Current token being processed

        // Sign should be the first character (only allowed after BEGIN)
        if (token->type == TOKEN_TYPE_SIGN) {
            // Check if this is the first significant token (immediately after BEGIN)
            if (i != 2) {
                // Sign not immediately after BEGIN
#ifdef VERBOSE
                fprintf(stderr, "Error: Sign '負' not at the beginning.\n");
#endif
                goto parse_error;
            }
        }


        // Disambiguate omitted 一 before 十/百/千/etc. (state MULT_AMBIG)
        // Disambiguate 零 before 十/百/千/etc. (state ZERO_MULT_AMBIG)
        switch (digit_state) {
        case DIGIT_STATE_MULT_AMBIG: // E.g., saw '十', now looking at previous token
            switch (token->type) {
            // <BEGIN>十 -> <BEGIN>一十 (10)
            // 負十 -> 負一十 (-10)
            // 又十 -> 又一十 (.10)
            // ·十 -> ·一十 (.10)
            // 分十絲 -> 分一十絲 (0.11) - Needs context of stack total exp
            case TOKEN_TYPE_BEGIN:
            case TOKEN_TYPE_SIGN:
            case TOKEN_TYPE_DELIM:
            case TOKEN_TYPE_DECIMAL:
            case TOKEN_TYPE_FRAC_MULT:
                // If previous token implies start of a number/section, insert '1'
                if (!result_push_digit(result, 1)) goto parse_error;
                digit_state = DIGIT_STATE_DIGIT;
                break;

            // 一十 -> 一十 (10) - No implicit 1 needed
            case TOKEN_TYPE_DIGIT:
                digit_state = DIGIT_STATE_MULT; // State becomes simple multiplier
                break;

            // 百(一?)十 -> 百一十 (110)
            // 千(一?)十 -> 千一十 (1010)
            // 萬(一?)十 -> 萬一十 (10010)
            // 萬(一?)百 -> 萬一百 (10100) - Handled by stack logic
            case TOKEN_TYPE_INT_MULT:
                // If new multiplier > stack top, implies separate units like 百(一?)十
                if (stack.count == 0 || mult_stack_top(&stack) < token->value.exp) {
                    if (!result_push_digit(result, 1)) goto parse_error;
                    digit_state = DIGIT_STATE_DIGIT;
                } else {
                    // e.g. 十萬 reading backwards
                    digit_state = DIGIT_STATE_MULT;
                }
                break;

            // 零(一?)十 -> 零一十 (010) - Insert implicit 1 after zero
            case TOKEN_TYPE_ZERO:
                if (!result_push_digit(result, 1)) goto parse_error;
                digit_state = DIGIT_STATE_DIGIT;
                break;

            default: // Should not happen
                break;
            }
            break; // End MULT_AMBIG disambiguation

        case DIGIT_STATE_ZERO_MULT_AMBIG: // E.g., saw '十', previous was '零'
            switch (token->type) {
            // <BEGIN>零十 -> <BEGIN>〇十 (error in strict traditional, maybe 10 or 0?) -> Assume 〇十 -> 0
            // 負(零一|零|〇)十 -> 負〇十 (-0)
            // 一(零一|零|〇)十 -> 一〇十 (100)
            // 又(零一|零|〇)十 -> 又〇十 (.0)
            // 零(零一|零|〇)十 -> 〇〇十 (0)
            case TOKEN_TYPE_BEGIN:
            case TOKEN_TYPE_SIGN:
            case TOKEN_TYPE_DIGIT: // e.g. 一(零十) -> 一百? or 一〇十? JS logic implies 一〇十
            case TOKEN_TYPE_DELIM:
            case TOKEN_TYPE_ZERO: // e.g., 零(零十) -> 零百? or 〇〇十? JS logic implies 〇〇十
                if (!result_push_digit(result, 0)) goto parse_error; // Push the effective 〇
                digit_state = DIGIT_STATE_DIGIT_WITH_ZERO; // Treat as if a digit followed by zero
                break;

            // ·零十釐 -> ·〇十釐 (0.00)
            // ·零十分 -> error? JS seems complex here. Let's simplify: Treat like digits.
            // 分零十絲 -> 分〇十絲 (0.100)
            // Simplified: Treat preceding 零 as 〇.
            case TOKEN_TYPE_DECIMAL:
            case TOKEN_TYPE_FRAC_MULT:
                if (mult_stack_total(&stack) + 1 < token->value.exp) {
                    if (!result_push_digit(result, 1)) goto parse_error;
                    if (!result_push_digit(result, 0)) goto parse_error;
                    digit_state = DIGIT_STATE_ZERO;
                } else if (mult_stack_total(&stack) + 1 == token->value.exp) {
                    if (!result_push_digit(result, 0)) goto parse_error;
                    digit_state = DIGIT_STATE_DIGIT_WITH_ZERO;
                } else {
                    goto parse_error;
                }
                break;


            // 千零十 -> 千〇十 (1000) - Reading backwards: 十 -> 零 -> 千
            // 百零十 -> 百〇十 (100)
            // 萬零萬 -> 萬〇萬 (100000000) - Ambiguous? JS allows this.
            case TOKEN_TYPE_INT_MULT:
                // // Simplified: Treat preceding 零 as 〇 in most cases before INT_MULT
                // if (!result_push_digit(result, 0)) goto parse_error;
                // digit_state = DIGIT_STATE_DIGIT_WITH_ZERO; // Treat as digit + zero state
                // // Complex JS logic for edge cases like 千零一十 vs 百〇十 vs 萬零萬 skipped for simplicity

                if (mult_stack_total(&stack) + 1 < token->value.exp) {
                    if (!result_push_digit(result, 1)) goto parse_error;
                    if (!result_push_digit(result, 0)) goto parse_error;
                    digit_state = DIGIT_STATE_ZERO;
                } else if (mult_stack_total(&stack) + 1 == token->value.exp) {
                    if (!result_push_digit(result, 0)) goto parse_error;
                    digit_state = DIGIT_STATE_DIGIT_WITH_ZERO;
                } else {
                    if (!result_push_digit(result, 0)) goto parse_error;
                    digit_state = DIGIT_STATE_ZERO;
                }

                break;
            default: // Should not happen
                break;
            }
            break; // End ZERO_MULT_AMBIG disambiguation
        default:
            break; // Not in an ambiguous state
        }


        // Determine the effective exponent for the current position based on the token
        // and update the multiplier stack accordingly.
        int current_exp = 0; // Exponent for the *lowest* place value covered by this token

        // Determine initial exponent if result is empty and seeing decimal/frac mult
        if (mult_stack_state(&stack) == MULT_STATE_NONE) {
            switch (token->type) {
            case TOKEN_TYPE_INT_MULT:
                // No reset needed, default 0 is fine
                break;
            case TOKEN_TYPE_DECIMAL: // Starts with '.' -> lowest exp is 0
            case TOKEN_TYPE_FRAC_MULT: // Starts with '分' -> lowest exp is -1
                result_reset_exp(result, token->value.exp);
                break;
            default: break;
            }
        }


        // Update multiplier stack and determine current effective exponent
        switch (token->type) {
        case TOKEN_TYPE_BEGIN:
        case TOKEN_TYPE_SIGN:
            if (digit_state == DIGIT_STATE_MULT) {
                // E.g., <BEGIN>微 is invalid
#ifdef VERBOSE
                fprintf(stderr, "Error: Dangling multiplier at the beginning.\n");
#endif
                goto parse_error;
            }
            if (!mult_stack_mark_done(&stack)) goto parse_error;
            current_exp = mult_stack_total(&stack); // Should be 0 if stack was just marked done
            break;

        case TOKEN_TYPE_DIGIT:
        case TOKEN_TYPE_ZERO:
            if (digit_state == DIGIT_STATE_DELIM) {
                // E.g., 一又 -> treats 又 as decimal point
                mult_stack_clear(&stack);
                if (!mult_stack_push(&stack, 0)) goto parse_error; // Decimal point implies exp 0 base
                current_exp = mult_stack_total(&stack);
            } else {
                // Digit follows digit/multiplier, use current result exponent
                // The exponent is the place value of the *next* digit to be added
                current_exp = result->exp;
            }
            break;

        case TOKEN_TYPE_DELIM: // 又 / 有
            if (digit_state == DIGIT_STATE_DELIM) {
                // 又又 is invalid
#ifdef VERBOSE
                fprintf(stderr, "Error: Consecutive delimiters '又'/'有'.\n");
#endif
                goto parse_error;
            }
            // Delimiter acts like a reset for exponents, similar to decimal point
            // Use current result exponent as the base for digits after it
            current_exp = result->exp;
            break;

        case TOKEN_TYPE_DECIMAL: // ·
        case TOKEN_TYPE_FRAC_MULT: // 分, 釐, ...
            if (digit_state == DIGIT_STATE_MULT) {
                // e.g. 微分 is invalid
#ifdef VERBOSE
                fprintf(stderr, "Error: Consecutive fractional multipliers or decimal point.\n");
#endif
                goto parse_error;
            }
            mult_stack_clear(&stack);
            if (!mult_stack_push(&stack, token->value.exp)) goto parse_error;
            current_exp = mult_stack_total(&stack);
            break;

        case TOKEN_TYPE_INT_MULT: // 十, 百, 千, 萬, ...
            if (digit_state == DIGIT_STATE_DELIM) {
                // e.g., 百又... -> treat 又 like decimal point
                if (mult_stack_state(&stack) == MULT_STATE_FRAC) {
                    mult_stack_clear(&stack); // Clear fractional state
                    if (!mult_stack_push(&stack, token->value.exp)) goto parse_error;
                } else {
                    // Remove smaller multipliers before pushing new one e.g. (萬->百)->億
                    while (stack.count > 0 && mult_stack_top(&stack) < token->value.exp) {
                        mult_stack_pop(&stack);
                    }
                    if (!mult_stack_push(&stack, token->value.exp)) goto parse_error;
                }
            } else if (digit_state != DIGIT_STATE_SIGN) {
                // Normal case: e.g., (reading backward) ...萬...億
                // Pop smaller *integer* multipliers
                while (stack.count > 0 &&
                    mult_stack_top(&stack) < token->value.exp &&
                    mult_stack_top(&stack) >= 0) // Don't pop fractional multipliers
                {
                    mult_stack_pop(&stack);
                }
                if (!mult_stack_push(&stack, token->value.exp)) goto parse_error;
            }
            current_exp = mult_stack_total(&stack);
            break;
        default: // Should not happen
            current_exp = mult_stack_total(&stack);
        }

        // if (current_exp < result->exp) {
        //     // Exponents should never go backwards
#ifdef VERBOSE
        //     fprintf(stderr, "Error: Exponents should never go backwards.\n");
#endif
        //     goto parse_error;
        // }

        // We're reading tokens backwards, so we need to check if the current token's exponent is
        // compatible with what we've parsed so far.

        // Check for special cases where exponent checks should be bypassed

        // Check for disallowed missing decimal places (fill zeros if needed)
        // `current_exp` is the exponent of the place value before this token.
        // `result->exp + result->count` is the place value of the next digit slot.
        if (current_exp > result->exp) {
            // We have a gap, e.g., reading "一百三" backwards:
            // 1. See '三', current_exp=0. Push 3. result=[3], exp=0, count=1. next_higher=1.
            // 2. See '百', stack=[2], current_exp=2. Needs filling. current_exp > next_higher (2 > 1).
            // Fill one zero. New exp = 0. New count = 2. Digits=[0,3].
            // Now next_higher = 0+2 = 2. current_exp == next_higher. Okay.

            bool allow_fill = false;
            switch (token->type) {
            case TOKEN_TYPE_BEGIN:
            case TOKEN_TYPE_SIGN:
            case TOKEN_TYPE_INT_MULT:
            case TOKEN_TYPE_FRAC_MULT:
            case TOKEN_TYPE_DECIMAL:
                allow_fill = true;
                break;
            default: // Digits, Zero, Delim
                // Allow fill only if previous state was conducive
                allow_fill = (digit_state == DIGIT_STATE_DELIM || digit_state == DIGIT_STATE_ZERO);
                break;
            }

            if (!allow_fill) {
#ifdef VERBOSE
                fprintf(stderr, "Error: Missing units between digits (gap %d -> %d).\n",
                    result->exp + (int)result->count, current_exp);
#endif
                goto parse_error;
            }

            if (mult_stack_state(&stack) != MULT_STATE_DONE) {
                // Add zeros
                const int zeros_needed = current_exp - result->exp;
                for (int k = 0; k < zeros_needed; ++k) {
                    if (!result_push_digit(result, 0)) goto parse_error;
                }
            }
        }


        // Push the digit (if any) and update parser state
        switch (token->type) {
        case TOKEN_TYPE_BEGIN:
            // Loop terminates before processing BEGIN
            break;

        case TOKEN_TYPE_SIGN:
            // Set the sign in result and continue processing
            result->negative = token->value.negative;
            digit_state = DIGIT_STATE_SIGN;
            break;

        case TOKEN_TYPE_DIGIT:
            if (!result_push_digit(result, token->value.digit)) goto parse_error;
            // Update state based on whether previous involved zero
            if (digit_state == DIGIT_STATE_ZERO || digit_state == DIGIT_STATE_DIGIT_WITH_ZERO) {
                digit_state = DIGIT_STATE_DIGIT_WITH_ZERO;
            } else {
                digit_state = DIGIT_STATE_DIGIT;
            }
            break;

        case TOKEN_TYPE_DECIMAL: // ·
            digit_state = DIGIT_STATE_NONE; // Reset state after decimal point
            break;

        case TOKEN_TYPE_INT_MULT: // 十, 百, 千, 萬, ...
            // If the multiplier implies a value (like 十), it might need a preceding '1'.
            // This is handled by the MULT_AMBIG state at the *start* of the next iteration.
            digit_state = DIGIT_STATE_MULT_AMBIG;
            break;

        case TOKEN_TYPE_FRAC_MULT: // 分, 釐, ...
            digit_state = DIGIT_STATE_MULT; // Fractional multipliers are not ambiguous
            break;

        case TOKEN_TYPE_DELIM: // 又 / 有
            digit_state = DIGIT_STATE_DELIM;
            break;

        case TOKEN_TYPE_ZERO: // 零
            // Special case: single "零" with nothing else in the result
            if (result->count == 0 && i == 1) {
                // Last token before BEGIN
                if (!result_push_digit(result, 0)) goto parse_error;
                digit_state = DIGIT_STATE_ZERO;
                break;
            }

            switch (digit_state) {
            case DIGIT_STATE_NONE:
            case DIGIT_STATE_MULT:
            case DIGIT_STATE_DIGIT:
            case DIGIT_STATE_DELIM:
            case DIGIT_STATE_DIGIT_WITH_ZERO:
            case DIGIT_STATE_ZERO:
                // Consecutive zeros - push another zero
                if (!result_push_digit(result, token->value.digit)) goto parse_error;
                digit_state = DIGIT_STATE_ZERO;
                break;
            case DIGIT_STATE_MULT_AMBIG: // Saw multiplier (e.g., 十), now see 零 -> 零十 ambiguity
                digit_state = DIGIT_STATE_ZERO_MULT_AMBIG;
                break;
            default: // Should not happen (e.g. SIGN state)
                goto parse_error;
            }
            break; // End ZERO case
        default: // Should not happen
            goto parse_error;
        }

        // if (digit_state == DIGIT_STATE_ZERO && token->type == TOKEN_TYPE_INT_MULT && i > 1 &&
        //     tokens[i - 2].type == TOKEN_TYPE_DIGIT) {
#ifdef VERBOSE
        //     fprintf(stderr, "Error: Invalid zero sequence (e.g., '一千零五百').\n");
#endif
        //     goto parse_error;
        // }
    } // End main parsing loop

    if (result->count == 0) {
#ifdef VERBOSE
        fprintf(stderr, "Error: No digits found in input.\n");
#endif
        goto parse_error; // Valid numbers must have digits
    }

    mult_stack_free(&stack);
    result->exp -= result->count; // Adjust exponent to account for removed digits
    return result;

parse_error:
    mult_stack_free(&stack);
    result_free(result);
    free(result);
    return NULL;
}

/* ---------- Convert ParseResult to double -------------------------------- */
#define FLOAT_MAX_FRAC (((uint64_t)1 << (FLT_MANT_DIG - 1)) - 1)
#define DOUBLE_MAX_FRAC (((uint64_t)1 << (DBL_MANT_DIG - 1)) - 1)

bool safe_x10(uint64_t* value) {
    // Check the remaining bits are enough to x10
    if (*value & 0xC000000000000000LLU)
        return true;
    const uint64_t cache = *value << 3,
                   newValue = cache + (*value << 1);
    // Check overflow
    if (newValue - cache != (*value << 1))
        return true;

    *value = newValue;
    return false;
}

bool safe_x10d(uint64_t* value) {
    uint64_t cache = *value;
    if (safe_x10(&cache))
        return true;
    if (cache > DOUBLE_MAX_FRAC)
        return true;
    *value = cache;
    return false;
}

void result_to_sci(const ParseResult* result, ScientificNotation* sciOut) {
    if (!result || result->count == 0) return;
    int exp = result->exp;
    int count = result->count;
    uint8_t* digits = result->digits;

    // Strip leading zeros from the digit array
    while (count > 0 && *digits == 0) {
        ++exp;
        --count;
        ++digits;
    }
    bool isNegative = result->negative;

    // Build integer value from digits, checking overflow
    uint64_t fraction = 0;
    uint32_t fractionLen = 0;
    for (int i = count - 1; i > -1; --i) {
        // printf("%c", digits[i] + '0');
        if (safe_x10(&fraction))
            break;

        // Check if overflow
        const uint64_t newFraction = fraction + digits[i];
        if (newFraction - fraction != digits[i])
            break;
        fraction = newFraction;
        if (fraction > 0)
            ++fractionLen;
    }
    // printf("\n");

    // Try to fit in i32 or i64
    bool isInt = false, isLong = false;
    if (exp >= 0) {
        // Check raw value limits
        if (isNegative) {
            isInt = fraction <= INT32_MAX + 1;
            isLong = fraction <= INT64_MAX + 1;
        } else {
            isInt = fraction <= INT32_MAX;
            isLong = fraction <= INT64_MAX;
        }

        // Try fit number into i32 or i64
        int cacheExp = exp;
        uint64_t cacheFraction = fraction;
        uint32_t cacheFractionLen = fractionLen;
        while (cacheExp > 0) {
            if (safe_x10(&cacheFraction)) {
                isInt = isLong = false;
                break;
            }
            ++cacheFractionLen;
            if (isNegative) {
                if (cacheFraction > (uint64_t)INT32_MAX + 1) isInt = false;
                if (cacheFraction > (uint64_t)INT64_MAX + 1) isLong = false;
            } else {
                if (cacheFraction > INT32_MAX) isInt = false;
                if (cacheFraction > INT64_MAX) isLong = false;
            }
            cacheExp--;
        }

        // If it fits, format as integer
        if (isInt || isLong) {
            fraction = cacheFraction;
            fractionLen = cacheFractionLen;
            exp = cacheExp;

            const int64_t intValue = isNegative ? -(int64_t)fraction : (int64_t)fraction;
            *sciOut = (ScientificNotation){isInt ? I32 : I64, intValue, fractionLen, exp};
            return;
        }
    }

    // If cant fit in i32 or i64, use double
    // TODO: Support float

    // Insure fraction is in the safe range, Scale the fraction down if too large, lose precision
    while (fraction > DOUBLE_MAX_FRAC) {
        fraction /= 10;
        --fractionLen;
        ++exp;
    }

    // Try to fit the exponent in double by scaling up if it's too small
    while (exp < DBL_MIN_10_EXP) {
        if (safe_x10d(&fraction))
            break;
        ++fractionLen;
        ++exp;
    }

    // if (exp < DBL_MIN_10_EXP || exp > DBL_MAX_10_EXP)
    // printf("double %s\n", value_str);
    const int64_t fractionValue = isNegative ? -(int64_t)fraction : (int64_t)fraction;
    *sciOut = (ScientificNotation){F64, fractionValue, fractionLen, exp};
}

char* sciToStr(const ScientificNotation* sci) {
    const bool neg = sci->fraction < 0;
    const uint32_t fracLen = sci->fractionLen + neg;

    switch (sci->type) {
    case I32:
    case I64:
        char* str = malloc((fracLen + 1) * sizeof(char));
        *str = '\0';
        snprintf(str, fracLen + 1, "%lld", sci->fraction);
        return str;
    case F64:
        // Convert digits to string buffer
        char* cache = malloc((sci->fractionLen + 1) * sizeof(char));
        *cache = '\0';
        snprintf(cache, sci->fractionLen + 1, "%lld", neg ? -sci->fraction : sci->fraction);
        // Write sign
        char* ptr;

        // Format depending on exponent vs. digits length
        if (sci->exp < 0 && -sci->exp == sci->fractionLen) {
            // Exact decimal fraction: 0.<digits>
            ptr = str = malloc((neg + sci->fractionLen + 2 + 1) * sizeof(char));
            if (neg) *ptr++ = '-';
            *ptr++ = '0';
            *ptr++ = '.';
            memcpy(ptr, cache, sci->fractionLen + 1);
        } else if (sci->exp < 0 && -sci->exp < sci->fractionLen) {
            // Insert decimal point within digits: 123.456
            ptr = str = malloc((neg + sci->fractionLen + 1 + 1) * sizeof(char));
            const int posCount = sci->fractionLen + sci->exp;
            if (neg) *ptr++ = '-';
            memcpy(ptr, cache, posCount);
            ptr += posCount;
            *ptr++ = '.';
            memcpy(ptr, cache + posCount, sci->fractionLen + 1 - posCount);
        } else {
            // Use scientific notation
            const bool extraFrac = sci->fractionLen > 1;
            int exp = sci->exp;
            if (extraFrac) {
                // display 123e0 -> 1.23e2, so add exp
                exp += sci->fractionLen - 1;
            }
            // Get exp length
            int expLen = (int)log10(exp < 0 ? -exp : exp) + 1;
            if (expLen < 2) expLen = 2;

            ptr = str = malloc((neg + extraFrac + sci->fractionLen + 3 + expLen + 1) * sizeof(char));
            if (neg) *ptr++ = '-';

            *ptr++ = *cache;
            if (extraFrac) {
                *ptr++ = '.';
                memcpy(ptr, cache + 1, sci->fractionLen);
                ptr += sci->fractionLen - 1;
            }

            *ptr++ = 'e';
            if (exp < 0) *ptr++ = '-';
            else *ptr++ = '+';

            *ptr = '\0';
            snprintf(ptr, expLen + 1, "%.2d", exp < 0 ? -exp : exp);
        }
        // printf("%d %d %s\n", exp, valueLen, cache);
        free(cache);
        return str;
    }
    return NULL;
}

double sciToDouble(const ScientificNotation* sci) {
    if (sci->type == ERROR) return NAN;

    double value = (double)sci->fraction;
    double scale = pow(10.0, sci->exp); // Start at lowest digit’s exponent
    value *= scale;

    return isfinite(value) ? value : (sci->fraction < 0 ? -INFINITY : INFINITY);
}


/* ---------- Main Converter (Revised) ----------------------------------- */
bool chineseToArabic(const char* utf8, ScientificNotation* sciOut) {
    char32_t* code = NULL;
    int len = 0;
    size_t token_count = 0;

    // Convert UTF8 to UTF32
    if ((len = utf8_to_u32(utf8, &code)) < 0) {
#ifdef VERBOSE
        fprintf(stderr, "Error: Invalid UTF-8 input.\n");
#endif
        sciOut->type = ERROR;
        return true;
    }
    if (len == 0) {
        free(code);
        sciOut->type = ERROR;
        return true;
    }

    // Tokenize the UTF32 string
    NumberToken* tokens = tokenize(code, len, &token_count);
    free(code); // Free UTF32 buffer now
    if (!tokens) {
        // Error message printed by tokenize
        sciOut->type = ERROR;
        return true;
    }

    // Parse the tokens
    ParseResult* result = parse_tokens(tokens, token_count);
    free(tokens); // Free token array now
    if (!result || result->count == 0) {
        // Error message printed by parse_tokens
        sciOut->type = ERROR;
        return true;
    }

    result_to_sci(result, sciOut);
    result_free(result); // Free ParseResult internal arrays
    free(result); // Free ParseResult struct itself

    return false;
}
