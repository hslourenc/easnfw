# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Imports --------------------------------------------------------------------
import os

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'EASNFW'
copyright = '2026, Henrique Sander Lourenço, João Victor Colombari Carlet'
author = 'Henrique Sander Lourenço, João Victor Colombari Carlet'
release = 'Pre-alpha v4'
today = 'August 24, 2026'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'sphinxcontrib.plantuml',
    'sphinx.ext.githubpages'
]

templates_path = ['_templates']
exclude_patterns = []



# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = 'classic'
html_static_path = ['_static']
numfig = True

# -- PlantUML configuration -------------------------------------------------
plantuml_jar = os.getenv('PLANTUML_JAR')
if not plantuml_jar:
    raise ValueError("PLANTUML_JAR environment variable is not set.")
plantuml = 'java -jar {}'.format(plantuml_jar)
