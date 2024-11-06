TESTS=$(sort $(basename $(wildcard generated*.lama)))
YAILama=../../YAILama
LAMAC=lamac

.PHONY: check $(TESTS)

check: $(TESTS)

$(TESTS): %: %.lama
	@echo "regression/deep-expressions/$@"
	@$(LAMAC) -b $<
	@cat $@.input | $(YAILama) $@.bc > $@.log && diff $@.log orig/$@.log

clean:
	rm -f *.log *.s *.bc *~
	find . -maxdepth 1 -type f -not -name '*.*' -not -name 'Makefile' -delete
