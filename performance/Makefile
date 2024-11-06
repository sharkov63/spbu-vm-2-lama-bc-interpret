TESTS=$(sort $(basename $(wildcard *.lama)))
YAILama=../YAILama
LAMAC=lamac

.PHONY: check $(TESTS)

check: $(TESTS)

$(TESTS): %: %.lama
	@echo $@
	@$(LAMAC) -b $<
	@$(LAMAC) $<
	`which time` -f "$@\t%U" ./$@
	`which time` -f "$@\t%U" $(YAILama) $@.bc
	`which time` -f "$@\t%U" $(LAMAC) -i $< < /dev/null

clean:
	$(RM) test*.log *.s *~ $(TESTS) *.i
