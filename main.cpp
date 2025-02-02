#include <iostream>
#include <string>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <stack>

enum InstructionType {
    NONE, // Default or uninitialized state
    MOV,    // copy value to the register, constant or value of a register
    INC,    // increase register value by one
    DEC,    // decrease register value by one
    ADD,    // add to the register constant or value of another register
    SUB,    // subtruct from the register constant or value of another register
    MUL,    // multiply register value by constant or another register value
    DIV,    // divide register value by constant or another register value
    JMP,    // jump to the label
    CMP,    // compares a register value to constant or value of another register
    // conditional jumps (jump to the label if result of CMP ...)
    JNE,    // ... is not equal
    JE,     // ... is equal
    JGE,    // ... is greater or equal
    JG,     // ... is greater
    JLE,    // ... is less or equal
    JL,     // ... is less
    CALL,   // call a subroutine
    RET,    // return instruction pointer to next instruction after CALL instruction
    MSG,    // stores the output of the program
    END     // end program and return stored value
    // comments are defined by the ';' symbol
};

class Interpreter
{
private:
    // stores the entire program as a string
    std::string program;
    // stores registers and their corresponding values
    std::unordered_map<std::string, int> regs;
    
    // stores the result of the CMP instruction
    int cmpResult;

    struct instruction {
        InstructionType type; 
        std::vector<std::string> args;
    };
    // stores subroutine labels and their corresponding sequences of instructions
    std::unordered_map<std::string, std::vector<Interpreter::instruction>> subroutines;
    // stores the main program as a list of instructions (including type and arguments)
    // instructions of subroutines are stored separately in `this->subroutines`
    std::vector<Interpreter::instruction> instructions;

    // stores the current pattern of message, which is outputted at the end of the program
    // the pattern consists of a sequence of register keys and literal text segments
    // the final message is constructed by replacing keys with corresponding register values and outputting the text exactly as stored
    std::vector<std::string> messagePattern;

    // stores the message returned by the interpreted assembler program
    std::string output;

    void initVariables();

    bool isConst(const std::string& str);
    bool isRegister(const std::string& str);

    void parseProgram();

    void execute();

    void createMessage();
public:
    Interpreter(const std::string& program);
    const std::string& getOutput() const;
};

// init functions
void Interpreter::initVariables()
{
    this->regs = {};
    this->cmpResult = 0;
    this->subroutines = {};
    this->instructions = {};
    this->messagePattern = {};
    this->output = "-1";
}

// functions
bool Interpreter::isConst(const std::string& str)
{
    // This function checks if a string represents a valid integer
    // Its behavior matches the regex: "^-?(0|[1-9]\\d*)$", which allows optional leading "-" and ensures no leading zeros unless the number is zero

    if (str.empty()) return false;
    if (str == "0" || str == "-0") return true;
    if (str == "-") return false;

    if (str.at(0) != '-' && (str.at(0) < '1' || str.at(0) > '9')) return false;

    for (size_t i = 1; i < str.length(); ++i) {
        if (str.at(i) < '0' || str.at(i) > '9') return false;
    }

    return true;
}

bool Interpreter::isRegister(const std::string& str)
{
    // This function checks if a string represents a valid register name
    // A valid register name consists only of lowercase alphabetic characters ('a' to 'z')
    // The behavior is equivalent to matching the regex pattern: "^[a-z]+$"

    for (char c : str) {
        if (c < 'a' || c > 'z') return false;
    }
    return true;
}

void Interpreter::parseProgram()
{
    // This function parses the program code stored in the `program` string.
    // It processes each line, identifying instructions, their arguments, and subroutine labels.
    // Comments starting with ';' are ignored, and leading/trailing whitespaces are trimmed.
    // Subroutines are stored in `this->subroutines`, while other instructions are stored in `this->instructions`.
    //
    // Key behaviors:
    // - Lines with no instructions or only comments are skipped.
    // - Subroutines (indicated by labels ending with ':') are stored as a sequence of instructions.
    // - Supported instructions are matched to their `InstructionType`. Unknown instructions are logged as errors and an exception is thrown.
    // - Instruction arguments are parsed.

    std::stringstream l_program(this->program);
    std::string line = "";

    auto rtrim = [&](const std::string& str) -> std::string {
        size_t pos = str.find_last_not_of(" \t\n\r\f\v");
        return pos == std::string::npos ? "" : str.substr(0, pos + 1);
    };
    auto ltrim = [&](const std::string& str) -> std::string {
        size_t pos = str.find_first_not_of(" \t\n\r\f\v");
        return pos == std::string::npos ? "" : str.substr(pos);
    };
    auto trim = [&](const std::string& str) -> std::string { return rtrim(ltrim(str)); };

    std::string subroutine = "";

    while (std::getline(l_program, line)) {
        // remove everything after first ';' (crop comments)
        line = [&](std::string& str) -> std::string {
            size_t pos = str.find(';');
            return pos == std::string::npos ? str : str.substr(0, pos);
        }(line);
        // remove whitespaces at the beginning and at the end of the string
        line = trim(line);

        // ignore empty lines
        if (line.length() == 0) continue;
        
        std::stringstream ss(line);
        std::string type = "";
        ss >> type;

        InstructionType instructionType = InstructionType::NONE;

        // set the current subroutine name (LABEL instruction)
        // all subsequent instructions will be associated with this subroutine
        if (type.length() > 1 && type.back() == ':') {
            subroutine = type.substr(0, type.length() - 1);
            this->subroutines[subroutine] = {};
            continue;
        }

        // a map which allows to convert string into enum (InstructionType)
        std::unordered_map<std::string, InstructionType> instructionTypeMap{
            { "mov", InstructionType::MOV },
            { "inc", InstructionType::INC },
            { "dec", InstructionType::DEC },
            { "add", InstructionType::ADD },
            { "sub", InstructionType::SUB },
            { "mul", InstructionType::MUL },
            { "div", InstructionType::DIV },
            { "jmp", InstructionType::JMP },
            { "cmp", InstructionType::CMP },
            { "jne", InstructionType::JNE },
            { "je", InstructionType::JE },
            { "jge", InstructionType::JGE },
            { "jg", InstructionType::JG },
            { "jle", InstructionType::JLE },
            { "jl", InstructionType::JL },
            { "call", InstructionType::CALL },
            { "msg", InstructionType::MSG },
            { "ret", InstructionType::RET },
            { "end", InstructionType::END }
        };

        // assigns the corresponding InstructionType based on the input string 'type'
        auto it = instructionTypeMap.find(type);
        if (it != instructionTypeMap.end()) {
            instructionType = it->second;
        } else {
            throw "ERROR::INTERPRETER::UNKOWN_INSTRUCTION_TYPE: " + type;
        }
        
        // parse args
        std::vector<std::string> args{};
        std::string arg = "";

        if (instructionType == InstructionType::MSG) {
            // msg instruction args are parsed differently, because they may include queted text
            bool insideQuote = false;
            
            for (char c : ss.str().substr(ss.tellg())) {
                // ingore leading whitespaces the first character is found 
                if (c == ' ' && arg.length() == 0) continue;
                // track if an arg is a text between apostrophes
                if (c == '\'') insideQuote = !insideQuote;
                // a comma indicates the end of the current argument, but only if it is outside quoted text
                else if (c == ',' && !insideQuote) {
                    args.push_back(arg);
                    arg = "";
                    continue;
                }
                arg += c;
            }
            // add the final arg to the list
            if (arg.length() > 0) args.push_back(arg);
        } else {
            // all other isntruction types
            while (ss >> arg) {
                // remove ',' from the end of the arg if exists
                arg = arg.back() == ',' ? arg.substr(0, arg.length() - 1) : arg;
                args.push_back(arg);
            }
        }

        // check if the current instruction is part of a subroutine
        if (subroutine == "") {
            // if no subroutine is active, add the instruction to the main instruction list
            this->instructions.push_back(Interpreter::instruction{instructionType, args});
        } else {
            // add instruction to the current subroutine
            // a subroutine is defined by a label and ends with a RET instruction or the start of another subroutine
            this->subroutines[subroutine].push_back(Interpreter::instruction{instructionType, args});
            // clear the subroutine name (RET instruction is an end of the subroutine definition)
            if (instructionType == InstructionType::RET) subroutine = "";
        }
    }
}

void Interpreter::execute()
{
    struct callFrame {
        std::vector<Interpreter::instruction> instructions;
        size_t instructionPointer;
        // bool calledSubroutine;
        
        callFrame(std::vector<Interpreter::instruction> instr)
            : instructions(std::move(instr)), instructionPointer(0) {} //, calledSubroutine(false) {};
    };
    std::stack<callFrame> call_stack{};

    call_stack.push(this->instructions);

    // bool isFrameFinishedByRet = false;

    while (!call_stack.empty()) {
        auto& frame = call_stack.top();

        if (frame.instructionPointer >= frame.instructions.size()) {
            // isFrameFinishedByRet = false;
            call_stack.pop(); // TODO fix this (dont pop, just continue with the next instruction)

            continue;
        }

        // if (frame.calledSubroutine) {
        //     if (isFrameFinishedByRet) {
        //         frame.calledSubroutine = false;
        //     } else {
        //         break;
        //     }
        // }

        Interpreter::instruction& instr = frame.instructions[frame.instructionPointer++];

        auto validateArgCount = [&](const size_t desiredSize) -> void {
            if (instr.args.size() != desiredSize) throw "ERROR::INTERPRETER::INVALID_NUMBER_OF_ARGS: " + std::to_string(instr.args.size());
        };
        auto validateFirstArgIsRegister = [&]() -> void {
            if (!this->isRegister(instr.args[0])) throw "ERROR::INTERPRETER::FIRST_ARG_SHOULD_BE_A_REGISTER: " + instr.args[0];
        };
        auto validateArgs = [&](const size_t desiredSize) -> void {
            validateArgCount(desiredSize);
            validateFirstArgIsRegister();
        };
        auto resolveValue = [&](const std::string& arg) -> int {
            int val = 0;
            if (this->isRegister(arg)) {
                val = this->regs[arg];
            } else if (this->isConst(arg)) {
                val = std::stoi(arg);
            } else {
                throw "ERROR::INTERPRETER::INVALID_ARG: " + arg;
            }
            return val;
        };
        auto findSubroutine = [&](const std::string& name) -> auto {
            auto it = this->subroutines.find(name);
            if (it == this->subroutines.end()) {
                throw "ERROR::INTERPRETER::CAN_NOT_FIND_SUBROUTINE: " + name;
            }
            return it->second;
        };
        auto replaceTopFrame = [&](const std::vector<Interpreter::instruction>& frame){
            call_stack.pop();
            call_stack.push(frame);
        };

        switch (instr.type)
        {
        case InstructionType::MOV:
            validateArgs(2);
            this->regs[instr.args[0]] = resolveValue(instr.args[1]);
            break;
        case InstructionType::INC:
            validateArgs(1);
            this->regs[instr.args[0]]++;
            break;
        case InstructionType::DEC:
            validateArgs(1);
            this->regs[instr.args[0]]--;
            break;
        case InstructionType::ADD:
            validateArgs(2);
            this->regs[instr.args[0]] += resolveValue(instr.args[1]);
            break;
        case InstructionType::SUB:
            validateArgs(2);
            this->regs[instr.args[0]] -= resolveValue(instr.args[1]);
            break;
        case InstructionType::MUL:
            validateArgs(2);
            this->regs[instr.args[0]] *= resolveValue(instr.args[1]); 
            break;
        case InstructionType::DIV:
            validateArgs(2);
            this->regs[instr.args[0]] /= resolveValue(instr.args[1]);
            break;
        case InstructionType::JMP:
            validateArgCount(1);
            replaceTopFrame(findSubroutine(instr.args[0]));
            continue;
        case InstructionType::CMP:
            validateArgCount(2);
            this-> cmpResult = resolveValue(instr.args[0]) - resolveValue(instr.args[1]);
            break;
        case InstructionType::JNE:
            validateArgCount(1);
            if (this->cmpResult != 0) {
                replaceTopFrame(findSubroutine(instr.args[0]));
            }
            continue;
        case InstructionType::JE:
            validateArgCount(1);
            if (this->cmpResult == 0) {
                replaceTopFrame(findSubroutine(instr.args[0]));
            }
            continue;
        case InstructionType::JGE:
            validateArgCount(1);
            if (this->cmpResult >= 0) {
                replaceTopFrame(findSubroutine(instr.args[0]));
            }
            continue;
        case InstructionType::JG:
            validateArgCount(1);
            if (this->cmpResult > 0) {
                replaceTopFrame(findSubroutine(instr.args[0]));
            }
            continue;
        case InstructionType::JLE:
            validateArgCount(1);
            if (this->cmpResult <= 0) {
                replaceTopFrame(findSubroutine(instr.args[0]));
            }
            continue;
        case InstructionType::JL:
            validateArgCount(1);
            if (this->cmpResult < 0) {
                replaceTopFrame(findSubroutine(instr.args[0]));
            }
            continue;
        case InstructionType::CALL:
            validateArgCount(1);
            // frame.calledSubroutine = true;
            call_stack.push(findSubroutine(instr.args[0]));
            break;
        case InstructionType::MSG:
            this->messagePattern = instr.args;
            break;
        case InstructionType::RET:
            // isFrameFinishedByRet = true;
            call_stack.pop();
            break;
        case InstructionType::END:
            this->createMessage();
            call_stack = {};
            break;
        default:
            break;
        }
    }
}

void Interpreter::createMessage()
{
    if (this->messagePattern.empty()) {
        // default output
        this->output = "-1";
        return;
    }

    this->output = "";
    for (std::string a : this->messagePattern) {
        if (a.at(0) == '\'') {
            // quoted text
            this->output += a.substr(1, a.length() - 2);
        } else if (this->isRegister(a)) {
            // register value
            this->output += std::to_string(this->regs[a]);
        } else {
            throw "ERROR::INTERPRETER::INVALID_MSG_ARGUMENT: " + a;
        }
    }
}

// constructor
Interpreter::Interpreter (const std::string& program)
    : program(program)
{
    this->initVariables();
    this->parseProgram();
    this->execute();
}

// accessors
const std::string& Interpreter::getOutput() const {
    return this->output;
};

std::string assembler_interpreter(std::string program) {
    Interpreter interpreter(program);
    return interpreter.getOutput();
}

int main ()
{
//     std::string program = R"(
// call  func1
// call  print
// end

// func1:
//     call  func2
//     ret

// func2:
//     ret

// print:
//     msg 'This program should return -1')";
std::string program = R"(
mov   a, 2            ; value1
mov   b, 10           ; value2
mov   c, a            ; temp1
mov   d, b            ; temp2
call  proc_func
call  print
end

proc_func:
    cmp   d, 1
    je    continue
    mul   c, a
    dec   d
    call  proc_func

continue:
    ret

print:
    msg a, '^', b, ' = ', c
    ret)";
    std::string program2 = R"(
mov   a, 81         ; value1
mov   b, 153        ; value2
call  init
call  proc_gcd
call  print
end

proc_gcd:
    cmp   c, d
    jne   loop
    ret

loop:
    cmp   c, d
    jg    a_bigger
    jmp   b_bigger

a_bigger:
    sub   c, d
    jmp   proc_gcd

b_bigger:
    sub   d, c
    jmp   proc_gcd

init:
    cmp   a, 0
    jl    a_abs
    cmp   b, 0
    jl    b_abs
    mov   c, a            ; temp1
    mov   d, b            ; temp2
    ret

a_abs:
    mul   a, -1
    jmp   init

b_abs:
    mul   b, -1
    jmp   init

print:
    msg   'gcd(', a, ', ', b, ') = ', c
    ret)";

    std::cout << "Program #1\n\n";

    std::string result = "";
    try {
        result = assembler_interpreter(program);
        std::cout << result << "\n\n";
    } catch (const std::string& e) {
        std::cout << e << std::endl;
    }
    

    // std::cout << "1. " << result << '\n';
    std::cout << "\nProgram #2\n\n";

    try {
        result = assembler_interpreter(program2);
        std::cout << result << "\n\n";
    } catch (const std::string& e) {
        std::cout << e << std::endl;
    }

    // std::cout << "2. " << result << '\n';
    return 0;
}
