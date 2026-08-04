PUML_DIR = ./src/plantuml

lualatex: ./src/requirements.tex plantuml
	cd ./src && lualatex --output-directory ../out ./requirements_clean.tex

plantuml: $(wildcard $(PUML_DIR)/*.puml)
	@if [ -z "$(PLANTUML_JAR)" ]; then \
		echo "Error: PLANTUML_JAR environment variable is not set" >&2; \
		exit 1; \
	fi
	@set -e; \
	for f in $(PUML_DIR)/*.puml; do \
		[ -e "$$f" ] || exit 0; \
		java -jar "$(PLANTUML_JAR)" -o "../images" "$$f"; \
	done
