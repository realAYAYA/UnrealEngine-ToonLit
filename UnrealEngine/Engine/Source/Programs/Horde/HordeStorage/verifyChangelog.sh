dir=$(dirname "${BASH_SOURCE[0]}")
current_version=$(yq e '.version' $dir/version.yaml)
cat $dir/changelog.md | grep -q $current_version && ( echo 'Version found' && exit 0 ) || (echo "No changelog section found for release $current_version" >&2 && exit 1)
       