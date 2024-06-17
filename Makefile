RM       = rm -rf
CLANG    = clang-14
LLVMLINK = llvm-link-14
LLI      = lli-14
ARMCC    = arm-linux-gnueabihf-gcc
QEMU     = qemu-arm

BUILD_DIR = $(CURDIR)/build
MAIN_EXE_LLVM  = $(BUILD_DIR)/tools/mainLLVM
MAIN_EXE_RPI = $(BUILD_DIR)/tools/mainRPi
TEST_DIR  = $(CURDIR)/test
FMJ2AST   = "$(CURDIR)/vendor/tools/fmj2ast"
AST2IRP   = "$(CURDIR)/vendor/tools/ast2irp"
IRP2LLVM  = "$(CURDIR)/vendor/tools/irp2llvm"

MAKEFLAGS = --no-print-directory

.PHONY: compile clean veryclean test-llvm test-rpi handin

compile:
	@cmake -G Ninja -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release; \
	cd $(BUILD_DIR) && ninja

clean:
	@find $(TEST_DIR) -type f \( \
    		-name "*.ll" -o -name "*.xml" -o -name "*.output" \
    		-o -name "*.src" -o -name "*.ast" -o -name "*.irp" \
    		-o -name "*.stm" -o -name "*.ins" -o -name "*.ssa" \
    		-o -name "*.cfg" -o -name "*.arm" -o -name "*.s" \
    		-o -name "*.itf" \
    		\) -exec $(RM) {} \;

veryclean: clean
	@$(RM) $(BUILD_DIR)

test-llvm: clean
	@cd $(TEST_DIR); \
    	if [ -z "$(TEST)" ]; then \
    		for file in $$(ls .); do \
    			if [ "$${file##*.}" = "fmj" ]; then \
    				echo "[$${file%%.*}]"; \
    				$(MAIN_EXE_LLVM) "$${file%%.*}" < "$${file%%.*}".fmj; \
    			fi \
    		done; \
    	else \
    		file=$(TEST); \
    		if [ "$${file##*.}" = "fmj" ]; then \
    			echo "[$${file%%.*}]"; \
    			$(MAIN_EXE_LLVM) "$${file%%.*}" < "$${file}"; \
    		else \
    			echo "Error: Specified file does not exist"; \
    			exit 1; \
    		fi \
    	fi; \
    cd $(CURDIR)

test-rpi: clean
	@cd $(TEST_DIR); \
        	if [ -z "$(TEST)" ]; then \
        		for file in $$(ls .); do \
        			if [ "$${file##*.}" = "fmj" ]; then \
        				echo "[$${file%%.*}]"; \
        				$(MAIN_EXE_RPI) "$${file%%.*}" < "$${file%%.*}".fmj; \
        			fi \
        		done; \
        	else \
        		file=$(TEST); \
        		if [ "$${file##*.}" = "fmj" ]; then \
        			echo "[$${file%%.*}]"; \
        			$(MAIN_EXE_RPI) "$${file%%.*}" < "$${file}"; \
        		else \
        			echo "Error: Specified file does not exist"; \
        			exit 1; \
        		fi \
        	fi; \
        cd $(CURDIR)

run-llvm: clean
	@cd $(TEST_DIR); \
    	if [ -z "$(TEST)" ]; then \
    		for file in $$(ls .); do \
    			if [ "$${file##*.}" = "fmj" ]; then \
    				echo "[$${file%%.*}]"; \
    				$(MAIN_EXE_LLVM) "$${file%%.*}" < "$${file%%.*}".fmj; \
    				$(LLVMLINK) --opaque-pointers "$${file%%.*}".7.ssa $(BUILD_DIR)/vendor/libsysy/libsysy64.ll -S -o "$${file%%.*}".ll && \
					$(LLI) -opaque-pointers "$${file%%.*}".ll > "$${file%%.*}".output && \
					echo $$?; \
    			fi \
    		done; \
    	else \
    		file=$(TEST); \
    		if [ "$${file##*.}" = "fmj" ]; then \
    			echo "[$${file%%.*}]"; \
    			$(MAIN_EXE_LLVM) "$${file%%.*}" < "$${file}"; \
    			$(LLVMLINK) --opaque-pointers "$${file%%.*}".7.ssa $(BUILD_DIR)/vendor/libsysy/libsysy64.ll -S -o "$${file%%.*}".ll && \
				$(LLI) -opaque-pointers "$${file%%.*}".ll > "$${file%%.*}".output && \
				echo $$?; \
    		else \
    			echo "Error: Specified file does not exist"; \
    			exit 1; \
    		fi \
    	fi; \
    cd $(CURDIR)

test-extra-run-rpi: clean
	@cd $(TEST_DIR)/extra; \
	for file in $$(ls .); do \
	  	if [ "$${file##*.}" = "fmj" ]; then \
			echo "[$${file%%.*}]"; \
			$(MAIN_EXE_RPI) "$${file%%.*}" < "$${file%%.*}".fmj; \
			$(ARMCC) -mcpu=cortex-a72 "$${file%%.*}".9.s $(BUILD_DIR)/vendor/libsysy/libsysy32.s --static -o "$${file%%.*}".s && \
            $(QEMU) -B 0x1000 "$${file%%.*}".s > "$${file%%.*}".output && \
            echo $$?; \
		fi \
	done; \
	cd $(CURDIR)

extra-llvm:
	@cd $(TEST_DIR)/extra; \
	$(LLVMLINK) --opaque-pointers test.7.ssa $(BUILD_DIR)/vendor/libsysy/libsysy64.ll -S -o test.ll && \
	$(LLI) -opaque-pointers test.ll > test.output && \
	echo $$?; \
	cd $(CURDIR)

handin:
	@if [ ! -f docs/report.pdf ]; then \
		echo "请先在docs文件夹下攥写报告, 并转换为'report.pdf'"; \
		exit 1; \
	fi; \
	echo "请输入'学号-姓名' (例如: 12345678910-某个人)"; \
	read filename; \
	zip -q -r "docs/$$filename-final.zip" \
	  include lib tools vendor test CMakeLists.txt docs/report.pdf
