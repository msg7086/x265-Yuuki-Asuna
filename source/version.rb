light = ARGV.first == 'light'

nickname = `git rev-parse --abbrev-ref HEAD`.strip
vbase, vhead, vhcommit = `git describe --tags HEAD`.strip.split('-')
branch = nickname == 'Yuuki' ? 'stable' : 'old-stable'
_, vmaster, vmcommit = `git describe --tags origin/#{branch}`.strip.split('-')

version = ''
tag = vbase.delete('M')

if vhcommit == vmcommit
  version = "#{vbase}+#{vmaster}-#{vmcommit}"
else
  vdiff = vhead.to_i - vmaster.to_i
  version = "#{vbase}+#{vmaster}-#{vmcommit}+#{vdiff}"
end
puts light ? tag : version
