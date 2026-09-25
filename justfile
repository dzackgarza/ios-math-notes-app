# Swift builds only on the macOS CI runner; locally, validate the YAML specs.
test-commit:
    uvx yamllint -s -d '{extends: relaxed, rules: {line-length: disable}}' project.yml .github/workflows/ios.yml

test-push: test-commit
