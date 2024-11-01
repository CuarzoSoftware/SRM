# Make sure to cd into this script dir
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Get date
WEEK_DAY=$(LANG=C date +%a | awk '{print toupper(substr($0, 1, 1)) substr($0, 2)}')
MONTH=$(LANG=C date +%b | awk '{print toupper(substr($0, 1, 1)) substr($0, 2)}')
MONTH_DAY=$(date +%d)
YEAR=$(date +%Y)

# Git repo base dir
PROJ_DIR=$SCRIPT_DIR/../../

# Get version
VERSION=$(cat $PROJ_DIR/VERSION)
MAJOR="${VERSION%%.*}"
MINOR="${VERSION#*.}"
MINOR="${MINOR%%.*}"
PATCH="${VERSION##*.}"
BUILD=$(cat $PROJ_DIR/BUILD)

# Get changelog
CHANGES=$(awk '/--/{exit} {print}' $PROJ_DIR/CHANGES)
CHANGES=$(echo "$CHANGES" | sed 's/^[ \t]*//') # Remove leading spaces
CHANGES=$(echo "$CHANGES" | sed 's/^*/-/g') # Replace * with -
CHANGES=$(echo "$CHANGES" | grep '^\-') # Filter out lines not starting with -

# Copy template
rm -f latest.spec
cp template.spec latest.spec

# Replace placeholders
sed -i "s/__MAJOR__/$MAJOR/g" latest.spec
sed -i "s/__MINOR__/$MINOR/g" latest.spec
sed -i "s/__PATCH__/$PATCH/g" latest.spec
sed -i "s/__BUILD__/$BUILD/g" latest.spec
sed -i "s/__WEEK_DAY__/$WEEK_DAY/g" latest.spec
sed -i "s/__MONTH__/$MONTH/g" latest.spec
sed -i "s/__MONTH_DAY__/$MONTH_DAY/g" latest.spec
sed -i "s/__YEAR__/$YEAR/g" latest.spec
echo "$CHANGES" >> latest.spec

# Print values
echo "Version:" $MAJOR.$MINOR.$PATCH-$BUILD
echo "Date:" $WEEK_DAY $MONTH $MONTH_DAY $YEAR
echo "Changes:"
echo "$CHANGES"

