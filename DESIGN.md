# Writing desk

The interface is a quiet desktop writing utility. A large, real mixed-language
preview is the focal content. Typography, alignment and precise feedback carry
the identity; ornamental dashboards and equal-size card grids do not.

Use the existing Qt/QML stack. A system sans face keeps controls familiar and
renders Vietnamese and German correctly. Hierarchy comes from a small type
scale, weight and space. Layout: persistent top status/master switch, slim text
navigation rail, scrollable work surface, quiet footer. At smaller sizes the
rail narrows, text wraps, and every action remains reachable.

Follow the system theme: warm paper surfaces in light mode and deep green ink
in dark mode. Teal is the actionable accent; errors use a distinct red. Colors,
spacing and type sizes live in qml/components/Theme.qml. Body text must maintain
4.5:1 contrast. State must also be expressed in words, never only color.

Use real Qt controls with keyboard focus, accessible names and explicit disabled
states. The practice input contains literal keys; its read-only result comes
from the same configured engine used for real typing. Personal words are edited
as a draft and saved explicitly. System instructions belong on Setup, where
they help resolve a real availability decision.

Keep motion to brief state feedback. Verify the normal and minimum-size window,
preferences and setup in a bounded review pass; batch material fixes before a
final confirmation capture.
